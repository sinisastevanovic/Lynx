#include "GpuScene.h"

#include "GraphicsContext.h"
#include "MaterialTable.h"
#include "Vulkan/UploadQueue.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/StaticMesh.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <iterator>
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

            // What the sun sees of each cascade of its shadow.
            CullView Sun[ShadowCascadeCount];
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

            // What is smaller than this many texels of a cascade's map casts no shadow into it.
            float SmallestCasterInTexels;

            // The world as the sun sees it, turned but not moved, and per cascade the square of ground
            // its map covers in that view: the middle in xy, half its width in z, and in w the size of
            // one of its texels.
            glm::mat4 SunView;
            glm::vec4 Cascades[ShadowCascadeCount];

            // The scattered instances, their kinds, the submeshes of the kinds (GpuScene::ScatterBatch),
            // and per instance the bits that say which of its submeshes the camera saw in the last frame.
            uint64_t ScatterInstances;
            uint64_t ScatterKinds;
            uint64_t ScatterBatches;
            uint64_t ScatterVisibility;
            uint32_t ScatterCount;
            uint32_t Padding;
        };

        // Where the list of scatter batches has the submeshes of a kind. Has to match ScatterKind in Cull.slang.
        struct GpuScatterKind
        {
            uint32_t FirstBatch = 0;
            uint32_t BatchCount = 0;
        };

        // The bits of a scattered instance that say what the camera saw have room for this many submeshes.
        constexpr uint32_t MaxSubmeshesOfScatterKind = 32;

        // Where the list of levels has those of a batch. No levels for a batch that culling on the GPU
        // does not draw. Has to match BatchLods in Cull.slang.
        struct BatchLods
        {
            uint32_t First = 0;
            uint32_t Count = 0;

            // For foliage: the size of a leaf, in the units a level's error is in. Zero for everything else.
            float LeafSize = 0.0f;
            uint32_t Padding = 0;
        };

        // One level of detail of a batch: its error, and the draw commands its instances are counted into,
        // the camera's for the two phases of culling and the sun's for each cascade of its shadow.
        // Has to match LodCommands in Cull.slang.
        struct LodCommands
        {
            float Error = 0.0f;
            uint32_t Camera = NoCommand;
            uint32_t CameraLate = NoCommand;
            uint32_t Sun[ShadowCascadeCount] = { NoCommand, NoCommand, NoCommand, NoCommand };
            uint32_t Padding = 0;
        };
        static_assert(ShadowCascadeCount == 4);

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
        glm::vec4 WorldSphere(const InstanceTransform& transform, const glm::vec4& sphere)
        {
            const glm::vec4* rows = transform.Rows;
            const glm::vec3 axisX(rows[0].x, rows[1].x, rows[2].x);
            const glm::vec3 axisY(rows[0].y, rows[1].y, rows[2].y);
            const glm::vec3 axisZ(rows[0].z, rows[1].z, rows[2].z);
            const float scale = std::sqrt(std::max({ glm::dot(axisX, axisX), glm::dot(axisY, axisY), glm::dot(axisZ, axisZ) }));

            const glm::vec4 centre(glm::vec3(sphere), 1.0f);
            return glm::vec4(glm::dot(rows[0], centre), glm::dot(rows[1], centre), glm::dot(rows[2], centre), sphere.w * scale);
        }

        // Writes the given places of a list to the buffer that holds the whole list. The places have to
        // be in rising order.
        //
        // Places that lie close to each other are written with one copy, the ones between them included.
        // A copy costs the GPU about a microsecond however small it is, measured with 20,000 of them,
        // which is more than copying a few dozen entries that did not change.
        template<typename T>
        void UploadPlaces(UploadQueue& uploads, const GpuBuffer& buffer, std::span<const uint32_t> places, const std::vector<T>& all)
        {
            size_t runStart = 0;
            for (size_t index = 1; index <= places.size(); ++index)
            {
                const bool runEnds = index == places.size() || places[index] > places[index - 1] + LargestGapInOneCopy;
                if (!runEnds)
                    continue;

                const uint32_t first = places[runStart];
                const uint32_t last = places[index - 1];
                uploads.Upload(buffer.Handle(), first * sizeof(T), std::as_bytes(std::span<const T>(&all[first], last - first + 1)));
                runStart = index;
            }
        }
    }

    GpuScene::GpuScene(GraphicsContext& context, UploadQueue& uploads, MaterialTable& materials)
        : m_Context(context), m_Uploads(uploads), m_Materials(materials)
    {
        static_assert(sizeof(GpuInstance) == 48);
        static_assert(sizeof(InstanceTransform) == 48);
    }

    GpuScene::~GpuScene() = default;

    void GpuScene::Clear()
    {
        m_Instances.clear();
        m_Transforms.clear();
        m_IndexInBatch.clear();
        m_Changed.clear();
        m_Moved.clear();
        m_MovedLastFrame.clear();
        m_Placed.clear();
        m_FreeRuns.clear();
        m_Batches.clear();
        m_FreeBatches.clear();
        m_BatchIndices.clear();
        m_DrawOrder.clear();
        m_DrawOrderIsStale = true;
        m_InstanceCount = 0;
        m_ScatterSets.clear();
        m_ScatterKinds.clear();
        m_ScatterBatches.clear();
        m_ScatterInstances.clear();
        m_ScatterIsStale = false;
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
            instance.BoundingSphere = submesh.BoundingSphere;
            instance.FirstIndex = submesh.Geometry.FirstIndex();
            instance.IndexCount = submesh.IndexCount;
            instance.VertexOffset = submesh.Geometry.VertexOffset();
            instance.Material = m_Materials.IndexOf(material);
            instance.EntityId = entityId;
            instance.Batch = batchIndex;
            instance.LeafData = submesh.LeafData ? submesh.LeafData->Address() : 0;
            m_Transforms[place] = InstanceTransform::From(transform);

            m_Changed.push_back(place);
            m_Placed.push_back(place);
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
            ReleaseBatchIfUnused(batchIndex);

            // Culling on the GPU goes over every place, so it has to be told that this one is empty.
            m_Instances[place].Batch = NoBatch;
            m_Changed.push_back(place);
        }

        m_FreeRuns[range.Count].push_back(range.First);
        m_InstanceCount -= range.Count;
    }

    void GpuScene::ReleaseBatchIfUnused(uint32_t batchIndex)
    {
        Batch& batch = m_Batches[batchIndex];
        if (!batch.Mesh || !batch.Places.empty() || batch.IsScattered)
            return;

        // Lets go of the mesh and the material. The batch's place in the list is used again by the next new batch.
        m_BatchIndices.erase(BatchKey{ batch.Mesh.get(), batch.SubmeshIndex, batch.Material.get() });
        m_FreeBatches.push_back(batchIndex);
        batch = Batch();
        m_DrawOrderIsStale = true;
    }

    uint32_t GpuScene::AddScatter(std::span<const std::shared_ptr<StaticMesh>> kinds, std::span<const ScatterInstance> instances)
    {
        const uint32_t scatter = m_NextScatter++;
        ScatterSet& set = m_ScatterSets[scatter];
        set.Kinds.assign(kinds.begin(), kinds.end());
        set.Instances.assign(instances.begin(), instances.end());
        m_ScatterIsStale = true;
        return scatter;
    }

    void GpuScene::RemoveScatter(uint32_t scatter)
    {
        if (m_ScatterSets.erase(scatter) > 0)
            m_ScatterIsStale = true;
    }

    uint64_t GpuScene::ScatterInstanceAddress()
    {
        if (m_ScatterIsStale)
            LayOutScatter();
        return m_ScatterInstances.empty() ? 0 : m_ScatterInstanceBuffer.Address();
    }

    uint32_t GpuScene::ScatterKindFor(const std::shared_ptr<StaticMesh>& mesh)
    {
        for (uint32_t index = 0; index < m_ScatterKinds.size(); ++index)
        {
            if (m_ScatterKinds[index].Mesh == mesh)
                return index;
        }
        if (m_ScatterKinds.size() >= ScatterInstance::KindCount)
            return ScatterInstance::NoKind;

        ScatterKind kind;
        kind.Mesh = mesh;
        kind.FirstBatch = static_cast<uint32_t>(m_ScatterBatches.size());

        const std::vector<Submesh>& submeshes = mesh->GetSubmeshes();
        for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size() && kind.BatchCount < MaxSubmeshesOfScatterKind; ++submeshIndex)
        {
            // See-through surfaces are sorted one by one on the CPU, which is not for a forest.
            const Submesh& submesh = submeshes[submeshIndex];
            if (!submesh.Geometry || !submesh.Material || submesh.Material->Mode == AlphaMode::Translucent)
                continue;

            const uint32_t batchIndex = BatchFor(mesh, submeshIndex, submesh.Material);
            m_Batches[batchIndex].IsScattered = true;

            // What every scattered instance of the submesh shares, in a place of its own. It names no
            // batch, so culling does not take the place for an instance that stands somewhere.
            const uint32_t place = TakePlaces(1);
            GpuInstance& shared = m_Instances[place];
            shared = {};
            shared.BoundingSphere = submesh.BoundingSphere;
            shared.FirstIndex = submesh.Geometry.FirstIndex();
            shared.IndexCount = submesh.IndexCount;
            shared.VertexOffset = submesh.Geometry.VertexOffset();
            shared.Material = m_Materials.IndexOf(submesh.Material);
            shared.EntityId = -1;
            shared.Batch = NoBatch;
            shared.LeafData = submesh.LeafData ? submesh.LeafData->Address() : 0;
            m_Transforms[place] = InstanceTransform::From(glm::mat4(1.0f));
            m_Changed.push_back(place);
            m_Placed.push_back(place);

            m_ScatterBatches.push_back({ batchIndex, place });
            ++kind.BatchCount;
        }

        m_ScatterKinds.push_back(std::move(kind));
        return static_cast<uint32_t>(m_ScatterKinds.size() - 1);
    }

    void GpuScene::LayOutScatter()
    {
        m_ScatterIsStale = false;

        // The kinds there were let go of what they held. Kinds that are still used take it again below.
        const std::vector<ScatterBatch> oldBatches = std::move(m_ScatterBatches);
        const std::vector<ScatterKind> oldKinds = std::move(m_ScatterKinds);
        m_ScatterBatches.clear();
        m_ScatterKinds.clear();
        m_ScatterInstances.clear();
        for (const ScatterBatch& old : oldBatches)
        {
            m_Batches[old.Batch].IsScattered = false;
            m_Batches[old.Batch].ScatterInstances = 0;
            m_FreeRuns[1].push_back(old.SharedPlace);
        }

        for (const auto& [scatter, set] : m_ScatterSets)
        {
            std::vector<uint32_t> kindOf(set.Kinds.size(), ScatterInstance::NoKind);
            for (size_t index = 0; index < set.Kinds.size(); ++index)
            {
                if (set.Kinds[index])
                    kindOf[index] = ScatterKindFor(set.Kinds[index]);
            }

            for (const ScatterInstance& instance : set.Instances)
            {
                const uint32_t kind = instance.Kind() < kindOf.size() ? kindOf[instance.Kind()] : ScatterInstance::NoKind;
                if (kind == ScatterInstance::NoKind)
                    continue;

                m_ScatterInstances.push_back(instance.WithKind(kind));
                ++m_ScatterKinds[kind].InstanceCount;
            }
        }

        for (const ScatterKind& kind : m_ScatterKinds)
        {
            for (uint32_t index = 0; index < kind.BatchCount; ++index)
                m_Batches[m_ScatterBatches[kind.FirstBatch + index].Batch].ScatterInstances = kind.InstanceCount;
        }

        // Only now: a batch that the kinds of before and those of now both use was never without a user.
        for (const ScatterBatch& old : oldBatches)
            ReleaseBatchIfUnused(old.Batch);

        if (m_ScatterInstances.empty())
            return;

        const vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        const std::span<const ScatterInstance> instances(m_ScatterInstances);
        EnsureSize(m_Context, m_ScatterInstanceBuffer, instances.size_bytes(), usage, "ScatterInstances");
        m_Uploads.Upload(m_ScatterInstanceBuffer.Handle(), 0, std::as_bytes(instances));

        // Nothing counts as seen in the last frame, so the first frame draws everything in the late phase.
        const std::vector<uint32_t> nothingSeen(m_ScatterInstances.size(), 0u);
        EnsureSize(m_Context, m_ScatterVisibilityBuffer, nothingSeen.size() * sizeof(uint32_t), usage, "ScatterVisibility");
        m_Uploads.Upload(m_ScatterVisibilityBuffer.Handle(), 0, std::as_bytes(std::span<const uint32_t>(nothingSeen)));
    }

    void GpuScene::SetTransform(Range range, const glm::mat4& transform)
    {
        const InstanceTransform rows = InstanceTransform::From(transform);
        for (uint32_t place = range.First; place < range.First + range.Count; ++place)
        {
            m_Transforms[place] = rows;
            m_Moved.push_back(place);
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
        assert(m_Instances.size() + count <= MaxPlaces);
        m_Instances.resize(m_Instances.size() + count);
        m_Transforms.resize(m_Instances.size());
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
        // This frame's transforms go into the buffer that held those of the frame before the last.
        GpuBuffer& transforms = m_TransformBuffers[m_Frame % 2];
        GpuBuffer& previousTransforms = m_TransformBuffers[(m_Frame + 1) % 2];

        const uint64_t neededSize = m_Instances.size() * sizeof(GpuInstance);
        if (m_InstanceBuffer.Size() < neededSize)
        {
            // Buffers of at least twice the size are filled with everything. Frames still on the GPU
            // keep reading the old ones, which live on until they are done.
            const size_t capacity = std::max(SmallestInstanceCapacity, std::bit_ceil(m_Instances.size()));
            GpuBufferDesc desc;
            desc.Size = capacity * sizeof(GpuInstance);
            desc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
            desc.Name = "SceneInstances";
            m_InstanceBuffer = GpuBuffer(m_Context, desc);
            m_Uploads.Upload(m_InstanceBuffer.Handle(), 0, std::as_bytes(std::span<const GpuInstance>(m_Instances)));

            // This frame's transforms are written whole. Last frame's are on the GPU only: what the
            // smaller buffer held is copied over there, and the instances that are new, which did not
            // move to where they are, are written after it.
            desc.Size = capacity * sizeof(InstanceTransform);
            desc.Name = "InstanceTransforms";
            transforms = GpuBuffer(m_Context, desc);
            m_Uploads.Upload(transforms.Handle(), 0, std::as_bytes(std::span<const InstanceTransform>(m_Transforms)));

            GpuBuffer grown(m_Context, desc);
            if (previousTransforms.Size() > 0)
                m_Uploads.MoveContents(std::move(previousTransforms), grown.Handle());
            previousTransforms = std::move(grown);
            std::ranges::sort(m_Placed);
            UploadPlaces(m_Uploads, previousTransforms, m_Placed, m_Transforms);

            // Nothing counts as seen in the last frame either, so the first frame with the new buffers
            // draws everything in the late phase.
            const std::vector<uint32_t> nothingSeen(capacity, 0u);
            desc.Size = nothingSeen.size() * sizeof(uint32_t);
            desc.Name = "InstanceVisibility";
            m_VisibilityBuffer = GpuBuffer(m_Context, desc);
            m_Uploads.Upload(m_VisibilityBuffer.Handle(), 0, std::as_bytes(std::span<const uint32_t>(nothingSeen)));

            // What moved in this frame is still missing in the buffer of the next one.
            std::ranges::sort(m_Moved);
            m_Moved.erase(std::ranges::unique(m_Moved).begin(), m_Moved.end());
            std::swap(m_Moved, m_MovedLastFrame);
            m_Moved.clear();
            m_Changed.clear();
            m_Placed.clear();
            return;
        }

        std::ranges::sort(m_Changed);
        UploadPlaces(m_Uploads, m_InstanceBuffer, m_Changed, m_Instances);
        m_Changed.clear();

        // What moved in the last frame was written to the other buffer then, and this one still has
        // where it was before that.
        std::ranges::sort(m_Moved);
        m_Moved.erase(std::ranges::unique(m_Moved).begin(), m_Moved.end());
        m_MovedInEitherFrame.clear();
        std::ranges::set_union(m_Moved, m_MovedLastFrame, std::back_inserter(m_MovedInEitherFrame));
        UploadPlaces(m_Uploads, transforms, m_MovedInEitherFrame, m_Transforms);
        std::swap(m_Moved, m_MovedLastFrame);
        m_Moved.clear();

        std::ranges::sort(m_Placed);
        UploadPlaces(m_Uploads, transforms, m_Placed, m_Transforms);
        UploadPlaces(m_Uploads, previousTransforms, m_Placed, m_Transforms);
        m_Placed.clear();
    }

    GpuScene::Frame GpuScene::Prepare(const CullCamera& camera, std::span<const ShadowCascade, ShadowCascadeCount> sun, const ShadowSettings& shadows, const glm::vec3& cameraPosition, const LodSettings& lods)
    {
        ++m_Frame;

        // The oldest copy is read before this frame takes its place.
        CommandCopy& copy = m_CommandCopies[m_Frame % CommandCopyCount];

        if (m_ScatterIsStale)
            LayOutScatter();

        Frame frame;
        frame.InstancesTested = m_InstanceCount + m_ScatterInstances.size();
        frame.ScatterInstances = m_ScatterInstances.size();
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

            // A leaf may be as many pixels large as the setting for foliage says, an error as many as the
            // one for everything else. The shader knows one scale, so the size of a leaf is brought to it here.
            const Submesh& submesh = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex];
            const std::vector<MeshLod>& levels = submesh.Lods;
            batchLods[index].First = static_cast<uint32_t>(lodCommands.size());
            batchLods[index].Count = static_cast<uint32_t>(levels.size());
            batchLods[index].LeafSize = submesh.LeafSize * lods.ErrorPixels / std::max(lods.LeafPixels, 0.01f);
            for (const MeshLod& level : levels)
                lodCommands.push_back({ .Error = level.Error });
        }

        // One draw command per batch, level and view, drawing nothing yet. Each gets a run in the list
        // of drawn instances that is long enough for the whole batch, the scattered instances that are
        // drawn with it included, because all of its instances may be drawn in one level. Culling on the
        // GPU counts the instances a view sees into the command of their level and writes them into its run.
        std::vector<vk::DrawIndexedIndirectCommand> commands;
        uint32_t visibleCount = 0;
        // commandOf says which of a level's commands is being laid out: that of one view.
        const auto addCommands = [&](auto&& commandOf)
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
                        commandOf(lodCommands[batchLods[index].First + level]) = static_cast<uint32_t>(commands.size());
                        commands.emplace_back(lod.IndexCount, 0u, submesh.Geometry.FirstIndex() + lod.FirstIndex, submesh.Geometry.VertexOffset(), visibleCount);
                        visibleCount += static_cast<uint32_t>(batch.Places.size()) + batch.ScatterInstances;
                    }
                }

                range.Count = static_cast<uint32_t>(commands.size()) - range.First;
            }
            return groups;
        };

        frame.Draws.CameraEarly = addCommands([](LodCommands& level) -> uint32_t& { return level.Camera; });
        frame.Draws.CameraLate = addCommands([](LodCommands& level) -> uint32_t& { return level.CameraLate; });
        for (uint32_t cascade = 0; cascade < ShadowCascadeCount; ++cascade)
            frame.Draws.Shadow[cascade] = addCommands([cascade](LodCommands& level) -> uint32_t& { return level.Sun[cascade]; });

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
            cullFrame.SunView = sun.front().SunView;
            for (uint32_t cascade = 0; cascade < ShadowCascadeCount; ++cascade)
            {
                Frustum seenBySun;
                seenBySun.FromViewProjection(sun[cascade].ViewProjection);
                cullFrame.Sun[cascade] = ToCullView(seenBySun);
                cullFrame.Cascades[cascade] = glm::vec4(sun[cascade].Centre, sun[cascade].Radius, sun[cascade].TexelSize);
            }
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
            cullFrame.SmallestCasterInTexels = shadows.SmallestCasterInTexels;

            std::vector<GpuScatterKind> scatterKinds;
            scatterKinds.reserve(m_ScatterKinds.size());
            for (const ScatterKind& kind : m_ScatterKinds)
                scatterKinds.push_back({ kind.FirstBatch, kind.BatchCount });

            // One after the other in one buffer: the frame, the batches, the levels, and for scattered
            // instances their kinds and the submeshes of the kinds.
            const std::span<const BatchLods> batchList(batchLods);
            const std::span<const LodCommands> lodList(lodCommands);
            const std::span<const GpuScatterKind> scatterKindList(scatterKinds);
            const std::span<const ScatterBatch> scatterBatchList(m_ScatterBatches);
            const uint64_t lodOffset = sizeof(CullFrame) + batchList.size_bytes();
            const uint64_t scatterKindOffset = lodOffset + lodList.size_bytes();
            const uint64_t scatterBatchOffset = scatterKindOffset + scatterKindList.size_bytes();
            EnsureSize(m_Context, m_CullInputBuffer, scatterBatchOffset + scatterBatchList.size_bytes(),
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "CullInput");

            cullFrame.ScatterCount = static_cast<uint32_t>(m_ScatterInstances.size());
            cullFrame.ScatterInstances = cullFrame.ScatterCount > 0 ? m_ScatterInstanceBuffer.Address() : 0;
            cullFrame.ScatterVisibility = cullFrame.ScatterCount > 0 ? m_ScatterVisibilityBuffer.Address() : 0;
            cullFrame.ScatterKinds = m_CullInputBuffer.Address() + scatterKindOffset;
            cullFrame.ScatterBatches = m_CullInputBuffer.Address() + scatterBatchOffset;
            cullFrame.Padding = 0;

            m_Uploads.Upload(m_CullInputBuffer.Handle(), 0, std::as_bytes(std::span<const CullFrame>(&cullFrame, 1)));
            m_Uploads.Upload(m_CullInputBuffer.Handle(), sizeof(CullFrame), std::as_bytes(batchList));
            m_Uploads.Upload(m_CullInputBuffer.Handle(), lodOffset, std::as_bytes(lodList));
            if (!scatterKinds.empty())
                m_Uploads.Upload(m_CullInputBuffer.Handle(), scatterKindOffset, std::as_bytes(scatterKindList));
            if (!m_ScatterBatches.empty())
                m_Uploads.Upload(m_CullInputBuffer.Handle(), scatterBatchOffset, std::as_bytes(scatterBatchList));

            frame.Culling.CommandAddress = m_CommandBuffer.Address();
            frame.Culling.FrameAddress = m_CullInputBuffer.Address();
            frame.Culling.BatchAddress = m_CullInputBuffer.Address() + sizeof(CullFrame);
            frame.Culling.LodAddress = m_CullInputBuffer.Address() + lodOffset;
            frame.Culling.VisibilityAddress = m_VisibilityBuffer.Address();
            frame.Culling.PlaceCount = static_cast<uint32_t>(m_Instances.size());
            frame.Culling.ScatterCount = cullFrame.ScatterCount;
        }

        frame.Draws.Buffer = &m_CommandBuffer;
        frame.InstanceAddress = m_InstanceBuffer.Address();
        frame.VisibleAddress = m_VisibleBuffer.Address();
        frame.TransformAddress = m_TransformBuffers[m_Frame % 2].Address();
        frame.PreviousTransformAddress = m_TransformBuffers[(m_Frame + 1) % 2].Address();
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
                const InstanceTransform& transform = m_Transforms[place];
                const glm::vec4 sphere = WorldSphere(transform, m_Instances[place].BoundingSphere);
                if (camera.IsOnFrustum(glm::vec3(sphere), sphere.w))
                    sorted.push_back({ glm::distance(cameraPosition, transform.Translation()), place });
            }
        }

        // Instances at the same distance keep the order of their places, so that a frame looks the same in every run.
        std::ranges::sort(sorted, [](const Sorted& a, const Sorted& b) { return std::tie(b.Distance, a.Place) < std::tie(a.Distance, b.Place); });

        // Their places follow the runs of the draw commands in the list of drawn instances.
        std::vector<DrawnEntry> places;
        places.reserve(sorted.size());
        m_TransparentDraws.clear();
        counted.SeeThroughInstances = sorted.size();
        counted.SeeThroughTriangles = 0;
        for (const Sorted& entry : sorted)
        {
            const GpuInstance& instance = m_Instances[entry.Place];
            m_TransparentDraws.push_back({ instance.IndexCount, instance.FirstIndex, instance.VertexOffset,
                firstVisible + static_cast<uint32_t>(places.size()), m_Batches[instance.Batch].Kind.IsTwoSided });
            places.push_back({ entry.Place, 0u });
            counted.SeeThroughTriangles += instance.IndexCount / 3;
        }

        // Room for every see-through instance, seen or not, so that the buffer does not grow as the camera turns.
        const uint64_t neededCount = firstVisible + seeThroughCount;
        if (neededCount == 0)
            return;

        EnsureSize(m_Context, m_VisibleBuffer, neededCount * sizeof(DrawnEntry), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "VisibleInstances");
        if (!places.empty())
            m_Uploads.Upload(m_VisibleBuffer.Handle(), firstVisible * sizeof(DrawnEntry), std::as_bytes(std::span<const DrawnEntry>(places)));
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
