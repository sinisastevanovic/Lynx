#include "GpuScene.h"

#include "GraphicsContext.h"
#include "MaterialTable.h"
#include "Vulkan/UploadQueue.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/StaticMesh.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <span>
#include <tuple>

namespace Lynx
{
    namespace
    {
        constexpr size_t SmallestInstanceCapacity = 1024;

        // How many unchanged instances may lie between two changed ones that are written with one copy.
        constexpr uint32_t LargestGapInOneCopy = 64;

        constexpr uint32_t NoCommand = ~0u;

        // The planes of a view, pointing inwards. Has to match View in Cull.slang.
        struct CullView
        {
            glm::vec4 Planes[6];
        };

        // What culling knows of the frame. Has to match CullFrame in Cull.slang.
        struct CullFrame
        {
            CullView Camera;
            CullView Sun;
            glm::mat4 View;

            // How much the camera's projection scales x and y, and the distance of its near plane.
            float ProjectionX;
            float ProjectionY;
            float NearPlane;

            // Only a perspective is tested against the depth pyramid, and only a perspective picks levels of detail.
            uint32_t IsPerspective;

            // A level is fine enough for an instance at a distance of at least its error times this.
            float LodScale;

            // The level every instance is drawn in. Negative for the level its distance picks.
            int32_t ForcedLevel;

            // How many levels coarser the sun's view draws.
            uint32_t ShadowBias;
            uint32_t Padding;
        };

        // Where the list of levels has those of a batch. No levels for a batch that culling on the GPU
        // does not draw. Has to match BatchLods in Cull.slang.
        struct BatchLods
        {
            uint32_t First = 0;
            uint32_t Count = 0;
        };

        // One level of detail of a batch: its error, and the draw commands its instances are counted into,
        // the camera's for the two phases of culling and the sun's. Has to match LodCommands in Cull.slang.
        struct LodCommands
        {
            float Error = 0.0f;
            uint32_t Camera = NoCommand;
            uint32_t CameraLate = NoCommand;
            uint32_t Sun = NoCommand;
        };

        CullView ToCullView(const Frustum& frustum)
        {
            CullView view;
            std::ranges::copy(frustum.Planes, view.Planes);
            return view;
        }

        // Makes sure a buffer that is rewritten every frame has a size. What it held is not kept.
        void EnsureSize(GraphicsContext& context, GpuBuffer& buffer, uint64_t size, vk::BufferUsageFlags usage, const char* name)
        {
            if (buffer.Size() >= size)
                return;

            // Half as much again as needed, so a list that grows a little each frame does not reallocate each frame.
            GpuBufferDesc desc;
            desc.Size = size + size / 2;
            desc.Usage = usage;
            desc.Name = name;
            buffer = GpuBuffer(context, desc);
        }

        // The material a submesh is drawn in: the one given for it, or else the mesh's own.
        const std::shared_ptr<Material>& MaterialOf(const std::vector<Submesh>& submeshes, std::span<const std::shared_ptr<Material>> materials, size_t submeshIndex)
        {
            const bool isGiven = submeshIndex < materials.size() && materials[submeshIndex];
            return isGiven ? materials[submeshIndex] : submeshes[submeshIndex].Material;
        }

        // The sphere around an instance in the world: the centre in xyz, the radius in w. The radius grows
        // by the largest of the three scales, which keeps the sphere around the mesh however unevenly
        // the instance is scaled. Cull.slang computes the same.
        glm::vec4 WorldSphere(const GpuInstance& instance)
        {
            const glm::mat4& model = instance.Model;
            const float scale = std::sqrt(std::max({ glm::dot(model[0], model[0]), glm::dot(model[1], model[1]), glm::dot(model[2], model[2]) }));
            const glm::vec3 centre = glm::vec3(model * glm::vec4(glm::vec3(instance.BoundingSphere), 1.0f));
            return glm::vec4(centre, instance.BoundingSphere.w * scale);
        }
    }

    GpuScene::GpuScene(GraphicsContext& context, UploadQueue& uploads, MaterialTable& materials)
        : m_Context(context), m_Uploads(uploads), m_Materials(materials)
    {
        static_assert(sizeof(GpuInstance) == 112);
    }

    GpuScene::~GpuScene() = default;

    void GpuScene::Clear()
    {
        m_Instances.clear();
        m_IndexInBatch.clear();
        m_Changed.clear();
        m_FreeRuns.clear();
        m_Batches.clear();
        m_FreeBatches.clear();
        m_BatchIndices.clear();
        m_DrawOrder.clear();
        m_DrawOrderIsStale = true;
        m_InstanceCount = 0;
    }

    GpuScene::Range GpuScene::Add(const std::shared_ptr<StaticMesh>& mesh, std::span<const std::shared_ptr<Material>> materials, const glm::mat4& transform, int entityId)
    {
        const std::vector<Submesh>& submeshes = mesh->GetSubmeshes();
        const auto canBeDrawn = [&](uint32_t submeshIndex)
        {
            return submeshes[submeshIndex].Geometry && MaterialOf(submeshes, materials, submeshIndex);
        };

        Range range;
        for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); ++submeshIndex)
            range.Count += canBeDrawn(submeshIndex) ? 1 : 0;
        if (range.Count == 0)
            return range;

        range.First = TakePlaces(range.Count);

        uint32_t place = range.First;
        for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); ++submeshIndex)
        {
            if (!canBeDrawn(submeshIndex))
                continue;

            const Submesh& submesh = submeshes[submeshIndex];
            const std::shared_ptr<Material>& material = MaterialOf(submeshes, materials, submeshIndex);
            const uint32_t batchIndex = BatchFor(mesh, submeshIndex, material);
            Batch& batch = m_Batches[batchIndex];
            m_IndexInBatch[place] = static_cast<uint32_t>(batch.Places.size());
            batch.Places.push_back(place);

            GpuInstance& instance = m_Instances[place];
            instance = {};
            instance.Model = transform;
            instance.BoundingSphere = submesh.BoundingSphere;
            instance.FirstIndex = submesh.Geometry.FirstIndex();
            instance.IndexCount = submesh.IndexCount;
            instance.VertexOffset = submesh.Geometry.VertexOffset();
            instance.Material = m_Materials.IndexOf(material);
            instance.EntityId = entityId;
            instance.Batch = batchIndex;

            m_Changed.push_back(place);
            ++place;
        }

        m_InstanceCount += range.Count;
        return range;
    }

    void GpuScene::Remove(Range range)
    {
        if (range.Count == 0)
            return;

        for (uint32_t place = range.First; place < range.First + range.Count; ++place)
        {
            const uint32_t batchIndex = m_Instances[place].Batch;
            Batch& batch = m_Batches[batchIndex];

            // The last place of the batch's list moves to where this one stood.
            const uint32_t movedPlace = batch.Places.back();
            batch.Places[m_IndexInBatch[place]] = movedPlace;
            m_IndexInBatch[movedPlace] = m_IndexInBatch[place];
            batch.Places.pop_back();

            if (batch.Places.empty())
            {
                // Lets go of the mesh and the material. The batch's place in the list is used again by the next new batch.
                m_BatchIndices.erase(BatchKey{ batch.Mesh.get(), batch.SubmeshIndex, batch.Material.get() });
                m_FreeBatches.push_back(batchIndex);
                batch = Batch();
                m_DrawOrderIsStale = true;
            }

            // Culling on the GPU goes over every place, so it has to be told that this one is empty.
            m_Instances[place].Batch = NoBatch;
            m_Changed.push_back(place);
        }

        m_FreeRuns[range.Count].push_back(range.First);
        m_InstanceCount -= range.Count;
    }

    void GpuScene::SetTransform(Range range, const glm::mat4& transform)
    {
        for (uint32_t place = range.First; place < range.First + range.Count; ++place)
        {
            m_Instances[place].Model = transform;
            m_Changed.push_back(place);
        }
    }

    uint32_t GpuScene::TakePlaces(uint32_t count)
    {
        // A run that was given back is taken again by a mesh with as many submeshes, which is what
        // nearly always follows: the same mesh, reloaded or added again.
        if (const auto runs = m_FreeRuns.find(count); runs != m_FreeRuns.end() && !runs->second.empty())
        {
            const uint32_t first = runs->second.back();
            runs->second.pop_back();
            return first;
        }

        const uint32_t first = static_cast<uint32_t>(m_Instances.size());
        m_Instances.resize(m_Instances.size() + count);
        m_IndexInBatch.resize(m_Instances.size());
        return first;
    }

    uint32_t GpuScene::BatchFor(const std::shared_ptr<StaticMesh>& mesh, uint32_t submeshIndex, const std::shared_ptr<Material>& material)
    {
        const auto [found, isNew] = m_BatchIndices.try_emplace(BatchKey{ mesh.get(), submeshIndex, material.get() }, 0u);
        if (!isNew)
            return found->second;

        if (m_FreeBatches.empty())
        {
            found->second = static_cast<uint32_t>(m_Batches.size());
            m_Batches.emplace_back();
        }
        else
        {
            found->second = m_FreeBatches.back();
            m_FreeBatches.pop_back();
        }

        Batch& batch = m_Batches[found->second];
        batch.Mesh = mesh;
        batch.SubmeshIndex = submeshIndex;
        batch.Material = material;
        m_DrawOrderIsStale = true;
        return found->second;
    }

    void GpuScene::UploadChanges()
    {
        const uint64_t neededSize = m_Instances.size() * sizeof(GpuInstance);
        if (m_InstanceBuffer.Size() < neededSize)
        {
            // A buffer of at least twice the size is filled with everything. Frames still on the GPU
            // keep reading the old one, which lives on until they are done.
            GpuBufferDesc desc;
            desc.Size = std::max(SmallestInstanceCapacity, std::bit_ceil(m_Instances.size())) * sizeof(GpuInstance);
            desc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
            desc.Name = "SceneInstances";
            m_InstanceBuffer = GpuBuffer(m_Context, desc);
            m_Uploads.Upload(m_InstanceBuffer.Handle(), 0, std::as_bytes(std::span<const GpuInstance>(m_Instances)));

            // Nothing counts as seen in the last frame, so the first frame with the new buffer draws
            // everything in the late phase. What was known is not carried over: buffers grow rarely.
            const std::vector<uint32_t> nothingSeen(desc.Size / sizeof(GpuInstance), 0u);
            desc.Size = nothingSeen.size() * sizeof(uint32_t);
            desc.Name = "InstanceVisibility";
            m_VisibilityBuffer = GpuBuffer(m_Context, desc);
            m_Uploads.Upload(m_VisibilityBuffer.Handle(), 0, std::as_bytes(std::span<const uint32_t>(nothingSeen)));
            m_Changed.clear();
            return;
        }

        if (m_Changed.empty())
            return;

        // Changed places that lie close to each other are written with one copy, the unchanged ones
        // between them included. A copy costs the GPU about a microsecond however small it is, measured
        // with 20,000 of them, which is more than copying a few dozen instances that did not change.
        std::ranges::sort(m_Changed);
        size_t runStart = 0;
        for (size_t index = 1; index <= m_Changed.size(); ++index)
        {
            const bool runEnds = index == m_Changed.size() || m_Changed[index] > m_Changed[index - 1] + LargestGapInOneCopy;
            if (!runEnds)
                continue;

            const uint32_t first = m_Changed[runStart];
            const uint32_t last = m_Changed[index - 1];
            m_Uploads.Upload(m_InstanceBuffer.Handle(), first * sizeof(GpuInstance), std::as_bytes(std::span<const GpuInstance>(&m_Instances[first], last - first + 1)));
            runStart = index;
        }
        m_Changed.clear();
    }

    GpuScene::Frame GpuScene::Prepare(const CullCamera& camera, const Frustum& sun, const glm::vec3& cameraPosition, const LodSettings& lods)
    {
        ++m_Frame;

        // The oldest copy is read before this frame takes its place.
        CommandCopy& copy = m_CommandCopies[m_Frame % CommandCopyCount];

        Frame frame;
        frame.InstancesTested = m_InstanceCount;
        ReadCounters(copy, frame);

        // A material is looked at once per frame: this is where a change to it is noticed and written to its table.
        for (Batch& batch : m_Batches)
        {
            if (!batch.Mesh)
                continue;

            m_Materials.IndexOf(batch.Material);
            batch.Kind.HasCutOuts = batch.Material->Mode == AlphaMode::Mask;
            batch.Kind.IsTwoSided = batch.Material->TwoSided;
            batch.IsTranslucent = batch.Material->Mode == AlphaMode::Translucent;
        }

        if (m_DrawOrderIsStale)
        {
            m_DrawOrder.clear();
            for (uint32_t index = 0; index < m_Batches.size(); ++index)
            {
                if (m_Batches[index].Mesh)
                    m_DrawOrder.push_back(index);
            }

            // Where two surfaces lie in the same plane, the one drawn last wins, so the order has to be
            // the same in every run for a frame to look the same every time. Handles give that.
            // First the batches of one submesh next to each other, by material.
            std::ranges::sort(m_DrawOrder, [this](uint32_t a, uint32_t b)
            {
                const uint64_t meshA = m_Batches[a].Mesh->GetHandle();
                const uint64_t meshB = m_Batches[b].Mesh->GetHandle();
                const uint64_t materialA = m_Batches[a].Material->GetHandle();
                const uint64_t materialB = m_Batches[b].Material->GetHandle();
                return std::tie(meshA, m_Batches[a].SubmeshIndex, materialA) < std::tie(meshB, m_Batches[b].SubmeshIndex, materialB);
            });

            // Then they are dealt out in turns: the first batch of every submesh, the second of every
            // submesh, and so on. The GPU draws a long run of instances of one mesh slower than the same
            // instances with other meshes in between. Measured in Crowd, four meshes in 64 materials each:
            // the depth pass took 0.71 ms with the 64 batches of a mesh in a row, and 0.54 ms dealt out.
            std::vector<uint32_t> turns(m_Batches.size(), 0);
            for (size_t index = 1; index < m_DrawOrder.size(); ++index)
            {
                const Batch& batch = m_Batches[m_DrawOrder[index]];
                const Batch& previous = m_Batches[m_DrawOrder[index - 1]];
                if (batch.Mesh == previous.Mesh && batch.SubmeshIndex == previous.SubmeshIndex)
                    turns[m_DrawOrder[index]] = turns[m_DrawOrder[index - 1]] + 1;
            }
            std::ranges::stable_sort(m_DrawOrder, [&turns](uint32_t a, uint32_t b) { return turns[a] < turns[b]; });
            m_DrawOrderIsStale = false;
        }

        // The levels of detail of every batch that culling on the GPU draws, one batch after the other.
        std::vector<BatchLods> batchLods(m_Batches.size());
        std::vector<LodCommands> lodCommands;
        for (const uint32_t index : m_DrawOrder)
        {
            const Batch& batch = m_Batches[index];
            if (batch.IsTranslucent)
                continue;

            const std::vector<MeshLod>& levels = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex].Lods;
            batchLods[index] = { static_cast<uint32_t>(lodCommands.size()), static_cast<uint32_t>(levels.size()) };
            for (const MeshLod& level : levels)
                lodCommands.push_back({ .Error = level.Error });
        }

        // One draw command per batch, level and view, drawing nothing yet. Each gets a run in the list
        // of drawn instances that is long enough for the whole batch, because all of its instances may
        // be drawn in one level. Culling on the GPU counts the instances a view sees into the command of
        // their level and writes their places into its run.
        std::vector<vk::DrawIndexedIndirectCommand> commands;
        uint32_t visibleCount = 0;
        const auto addCommands = [&](uint32_t LodCommands::* view)
        {
            // The commands of a view are grouped by kind, so that a pass draws each kind with one call.
            MeshDrawGroups groups;
            for (uint32_t kind = 0; kind < MeshDrawKind::Count; ++kind)
            {
                DrawRange& range = groups.Ranges[kind];
                range.First = static_cast<uint32_t>(commands.size());

                for (const uint32_t index : m_DrawOrder)
                {
                    // See-through surfaces cast no shadow, and the camera's are drawn one by one.
                    const Batch& batch = m_Batches[index];
                    if (batch.IsTranslucent || batch.Kind.Index() != kind)
                        continue;

                    const Submesh& submesh = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex];
                    for (uint32_t level = 0; level < submesh.Lods.size(); ++level)
                    {
                        const MeshLod& lod = submesh.Lods[level];
                        lodCommands[batchLods[index].First + level].*view = static_cast<uint32_t>(commands.size());
                        commands.emplace_back(lod.IndexCount, 0u, submesh.Geometry.FirstIndex() + lod.FirstIndex, submesh.Geometry.VertexOffset(), visibleCount);
                        visibleCount += static_cast<uint32_t>(batch.Places.size());
                    }
                }

                range.Count = static_cast<uint32_t>(commands.size()) - range.First;
            }
            return groups;
        };

        frame.Draws.CameraEarly = addCommands(&LodCommands::Camera);
        frame.Draws.CameraLate = addCommands(&LodCommands::CameraLate);
        frame.Draws.Shadow = addCommands(&LodCommands::Sun);

        copy.CommandCount = static_cast<uint32_t>(commands.size());
        copy.CameraCommandCount = frame.Draws.Camera().Count;

        CullSeeThrough(camera.Planes, cameraPosition, visibleCount, copy);
        frame.TransparentDraws = m_TransparentDraws;

        UploadChanges();

        if (!commands.empty())
        {
            // Read by draw calls, written by the upload and by culling, and copied from for the counters.
            const vk::BufferUsageFlags commandUsage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer
                | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc;
            const std::span<const vk::DrawIndexedIndirectCommand> commandList(commands);
            EnsureSize(m_Context, m_CommandBuffer, commandList.size_bytes(), commandUsage, "MeshDrawCommands");
            m_Uploads.Upload(m_CommandBuffer.Handle(), 0, std::as_bytes(commandList));

            // The near plane's distance, read from the projection the way the renderer does for reversed depth.
            const glm::mat4& projection = camera.Projection;
            CullFrame cullFrame;
            cullFrame.Camera = ToCullView(camera.Planes);
            cullFrame.Sun = ToCullView(sun);
            cullFrame.View = camera.View;
            cullFrame.ProjectionX = projection[0][0];
            cullFrame.ProjectionY = projection[1][1];
            cullFrame.IsPerspective = projection[2][3] != 0.0f ? 1u : 0u;
            cullFrame.NearPlane = cullFrame.IsPerspective ? projection[3][2] / projection[2][2] : 0.0f;

            // An error of e units at a distance of d covers e / d * projection * half the height in pixels.
            // A level may be drawn while that is no more than the pixels allowed, so from this distance on.
            const float pixelsPerUnitAtDistanceOne = std::abs(cullFrame.ProjectionY) * camera.ViewportHeight * 0.5f;
            cullFrame.LodScale = pixelsPerUnitAtDistanceOne / std::max(lods.ErrorPixels, 0.01f);
            cullFrame.ForcedLevel = lods.ForcedLevel;
            cullFrame.ShadowBias = lods.ShadowBias;
            cullFrame.Padding = 0;

            const std::span<const BatchLods> batchList(batchLods);
            const std::span<const LodCommands> lodList(lodCommands);
            const uint64_t lodOffset = sizeof(CullFrame) + batchList.size_bytes();
            EnsureSize(m_Context, m_CullInputBuffer, lodOffset + lodList.size_bytes(),
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "CullInput");
            m_Uploads.Upload(m_CullInputBuffer.Handle(), 0, std::as_bytes(std::span<const CullFrame>(&cullFrame, 1)));
            m_Uploads.Upload(m_CullInputBuffer.Handle(), sizeof(CullFrame), std::as_bytes(batchList));
            m_Uploads.Upload(m_CullInputBuffer.Handle(), lodOffset, std::as_bytes(lodList));

            frame.Culling.CommandAddress = m_CommandBuffer.Address();
            frame.Culling.FrameAddress = m_CullInputBuffer.Address();
            frame.Culling.BatchAddress = m_CullInputBuffer.Address() + sizeof(CullFrame);
            frame.Culling.LodAddress = m_CullInputBuffer.Address() + lodOffset;
            frame.Culling.VisibilityAddress = m_VisibilityBuffer.Address();
            frame.Culling.PlaceCount = static_cast<uint32_t>(m_Instances.size());
        }

        frame.Draws.Buffer = &m_CommandBuffer;
        frame.InstanceAddress = m_InstanceBuffer.Address();
        frame.VisibleAddress = m_VisibleBuffer.Address();
        return frame;
    }

    void GpuScene::CullSeeThrough(const Frustum& camera, const glm::vec3& cameraPosition, uint32_t firstVisible, CommandCopy& counted)
    {
        // See-through instances are drawn one by one, the farthest first, so that nearer ones cover them.
        struct Sorted
        {
            float Distance;
            uint32_t Place;
        };
        std::vector<Sorted> sorted;
        size_t seeThroughCount = 0;
        for (const uint32_t index : m_DrawOrder)
        {
            const Batch& batch = m_Batches[index];
            if (!batch.IsTranslucent)
                continue;

            seeThroughCount += batch.Places.size();
            for (const uint32_t place : batch.Places)
            {
                const GpuInstance& instance = m_Instances[place];
                const glm::vec4 sphere = WorldSphere(instance);
                if (camera.IsOnFrustum(glm::vec3(sphere), sphere.w))
                    sorted.push_back({ glm::distance(cameraPosition, glm::vec3(instance.Model[3])), place });
            }
        }

        // Instances at the same distance keep the order of their places, so that a frame looks the same in every run.
        std::ranges::sort(sorted, [](const Sorted& a, const Sorted& b) { return std::tie(b.Distance, a.Place) < std::tie(a.Distance, b.Place); });

        // Their places follow the runs of the draw commands in the list of drawn instances.
        std::vector<uint32_t> places;
        places.reserve(sorted.size());
        m_TransparentDraws.clear();
        counted.SeeThroughInstances = sorted.size();
        counted.SeeThroughTriangles = 0;
        for (const Sorted& entry : sorted)
        {
            const GpuInstance& instance = m_Instances[entry.Place];
            m_TransparentDraws.push_back({ instance.IndexCount, instance.FirstIndex, instance.VertexOffset,
                firstVisible + static_cast<uint32_t>(places.size()), m_Batches[instance.Batch].Kind.IsTwoSided });
            places.push_back(entry.Place);
            counted.SeeThroughTriangles += instance.IndexCount / 3;
        }

        // Room for every see-through instance, seen or not, so that the buffer does not grow as the camera turns.
        const uint64_t neededCount = firstVisible + seeThroughCount;
        if (neededCount == 0)
            return;

        EnsureSize(m_Context, m_VisibleBuffer, neededCount * sizeof(uint32_t), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "VisibleInstances");
        if (!places.empty())
            m_Uploads.Upload(m_VisibleBuffer.Handle(), firstVisible * sizeof(uint32_t), std::as_bytes(std::span<const uint32_t>(places)));
    }

    void GpuScene::ReadCounters(const CommandCopy& copy, Frame& frame) const
    {
        frame.InstancesDrawn = copy.SeeThroughInstances;
        frame.TrianglesDrawn = copy.SeeThroughTriangles;

        const std::span<const vk::DrawIndexedIndirectCommand> commands(static_cast<const vk::DrawIndexedIndirectCommand*>(copy.Commands.Data()), copy.CommandCount);
        if (copy.Commands.Size() < commands.size_bytes())
            return;

        for (uint32_t index = 0; index < commands.size(); ++index)
        {
            const vk::DrawIndexedIndirectCommand& command = commands[index];
            if (command.instanceCount == 0)
                continue;

            const bool isCamera = index < copy.CameraCommandCount;
            (isCamera ? frame.InstancesDrawn : frame.ShadowInstancesDrawn) += command.instanceCount;
            (isCamera ? frame.TrianglesDrawn : frame.ShadowTrianglesDrawn) += uint64_t(command.indexCount / 3) * command.instanceCount;
            frame.DrawCommands++;
        }
    }

    void GpuScene::CopyCommandsForCounters(vk::CommandBuffer commands)
    {
        CommandCopy& copy = m_CommandCopies[m_Frame % CommandCopyCount];
        if (copy.CommandCount == 0)
            return;

        const uint64_t size = copy.CommandCount * sizeof(vk::DrawIndexedIndirectCommand);
        if (copy.Commands.Size() < size)
        {
            GpuBufferDesc desc;
            desc.Size = size + size / 2;
            desc.Usage = vk::BufferUsageFlagBits::eTransferDst;
            desc.CpuReadable = true;
            desc.Name = "MeshDrawCommandsForCounters";
            copy.Commands = GpuBuffer(m_Context, desc);
        }

        commands.copyBuffer(m_CommandBuffer.Handle(), copy.Commands.Handle(), vk::BufferCopy(0, 0, size));
    }
}
