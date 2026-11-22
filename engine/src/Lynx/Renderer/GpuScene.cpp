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

        // The draw commands a batch's instances are counted into, one per view. NoCommand for a view the
        // batch is not drawn in. Has to match BatchCommands in Cull.slang.
        struct BatchCommands
        {
            uint32_t Camera = NoCommand;
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

        bool CanBeDrawn(const Submesh& submesh)
        {
            return submesh.Material && submesh.Geometry;
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

    GpuScene::Range GpuScene::Add(const std::shared_ptr<StaticMesh>& mesh, const glm::mat4& transform, int entityId)
    {
        const std::vector<Submesh>& submeshes = mesh->GetSubmeshes();

        Range range;
        range.Count = static_cast<uint32_t>(std::ranges::count_if(submeshes, CanBeDrawn));
        if (range.Count == 0)
            return range;

        range.First = TakePlaces(range.Count);

        uint32_t place = range.First;
        for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); ++submeshIndex)
        {
            const Submesh& submesh = submeshes[submeshIndex];
            if (!CanBeDrawn(submesh))
                continue;

            const uint32_t batchIndex = BatchFor(mesh, submeshIndex);
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
            instance.Material = m_Materials.IndexOf(submesh.Material);
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
                // Lets go of the mesh. The batch's place in the list is used again by the next new batch.
                m_BatchIndices.erase(BatchKey{ batch.Mesh.get(), batch.SubmeshIndex });
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

    uint32_t GpuScene::BatchFor(const std::shared_ptr<StaticMesh>& mesh, uint32_t submeshIndex)
    {
        const auto [found, isNew] = m_BatchIndices.try_emplace(BatchKey{ mesh.get(), submeshIndex }, 0u);
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

    GpuScene::Frame GpuScene::Prepare(const Frustum& camera, const Frustum& sun, const glm::vec3& cameraPosition)
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

            const std::shared_ptr<Material>& material = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex].Material;
            m_Materials.IndexOf(material);
            batch.HasCutOuts = material->Mode == AlphaMode::Mask;
            batch.IsTranslucent = material->Mode == AlphaMode::Translucent;
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
            // the same in every run for a frame to look the same every time.
            std::ranges::sort(m_DrawOrder, [this](uint32_t a, uint32_t b)
            {
                const uint64_t meshA = m_Batches[a].Mesh->GetHandle();
                const uint64_t meshB = m_Batches[b].Mesh->GetHandle();
                return std::tie(meshA, m_Batches[a].SubmeshIndex) < std::tie(meshB, m_Batches[b].SubmeshIndex);
            });
            m_DrawOrderIsStale = false;
        }

        // One draw command per batch and view, drawing nothing yet. Each gets a run in the list of drawn
        // instances that is long enough for the whole batch. Culling on the GPU counts the instances a
        // view sees into the command and writes their places into its run.
        std::vector<vk::DrawIndexedIndirectCommand> commands;
        std::vector<BatchCommands> batchCommands(m_Batches.size());
        uint32_t visibleCount = 0;
        const auto addCommands = [&](bool forCamera, bool withCutOuts)
        {
            DrawRange range;
            range.First = static_cast<uint32_t>(commands.size());

            for (const uint32_t index : m_DrawOrder)
            {
                // See-through surfaces cast no shadow, and the camera's are drawn one by one.
                const Batch& batch = m_Batches[index];
                if (batch.IsTranslucent || batch.HasCutOuts != withCutOuts)
                    continue;

                (forCamera ? batchCommands[index].Camera : batchCommands[index].Sun) = static_cast<uint32_t>(commands.size());

                const Submesh& submesh = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex];
                commands.emplace_back(submesh.IndexCount, 0u, submesh.Geometry.FirstIndex(), submesh.Geometry.VertexOffset(), visibleCount);
                visibleCount += static_cast<uint32_t>(batch.Places.size());
            }

            range.Count = static_cast<uint32_t>(commands.size()) - range.First;
            return range;
        };

        frame.Draws.CameraOpaque = addCommands(true, false);
        frame.Draws.CameraCutOut = addCommands(true, true);
        frame.Draws.ShadowOpaque = addCommands(false, false);
        frame.Draws.ShadowCutOut = addCommands(false, true);

        copy.CommandCount = static_cast<uint32_t>(commands.size());
        copy.CameraCommandCount = frame.Draws.CameraOpaque.Count + frame.Draws.CameraCutOut.Count;

        CullSeeThrough(camera, cameraPosition, visibleCount, copy);
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

            const CullView views[2] = { ToCullView(camera), ToCullView(sun) };
            const std::span<const BatchCommands> batchList(batchCommands);
            EnsureSize(m_Context, m_CullInputBuffer, sizeof(views) + batchList.size_bytes(),
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "CullInput");
            m_Uploads.Upload(m_CullInputBuffer.Handle(), 0, std::as_bytes(std::span(views)));
            m_Uploads.Upload(m_CullInputBuffer.Handle(), sizeof(views), std::as_bytes(batchList));

            frame.Culling.CommandAddress = m_CommandBuffer.Address();
            frame.Culling.ViewAddress = m_CullInputBuffer.Address();
            frame.Culling.BatchAddress = m_CullInputBuffer.Address() + sizeof(views);
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
            m_TransparentDraws.push_back({ instance.IndexCount, instance.FirstIndex, instance.VertexOffset, firstVisible + static_cast<uint32_t>(places.size()) });
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
