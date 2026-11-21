#include "GpuScene.h"

#include "GraphicsContext.h"
#include "MaterialTable.h"
#include "Vulkan/UploadQueue.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/StaticMesh.h"

#include <algorithm>
#include <bit>
#include <span>
#include <tuple>

namespace Lynx
{
    namespace
    {
        constexpr size_t SmallestInstanceCapacity = 1024;

        // How many unchanged instances may lie between two changed ones that are written with one copy.
        constexpr uint32_t LargestGapInOneCopy = 64;

        // Makes sure a buffer that is rewritten every frame can hold a list, then queues the list for upload.
        template<typename Entry>
        void UploadList(GraphicsContext& context, UploadQueue& uploads, GpuBuffer& buffer, std::span<const Entry> entries, vk::BufferUsageFlagBits usage, const char* name)
        {
            if (entries.empty())
                return;

            if (buffer.Size() < entries.size_bytes())
            {
                // Half as much again as needed, so a list that grows a little each frame does not reallocate each frame.
                GpuBufferDesc desc;
                desc.Size = entries.size_bytes() + entries.size_bytes() / 2;
                desc.Usage = usage | vk::BufferUsageFlagBits::eTransferDst;
                desc.Name = name;
                buffer = GpuBuffer(context, desc);
            }

            uploads.Upload(buffer.Handle(), 0, std::as_bytes(entries));
        }

        bool CanBeDrawn(const Submesh& submesh)
        {
            return submesh.Material && submesh.Geometry;
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
        m_CullData.clear();
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

            GpuInstance& instance = m_Instances[place];
            instance = {};
            instance.Model = transform;
            instance.BoundingSphere = submesh.BoundingSphere;
            instance.FirstIndex = submesh.Geometry.FirstIndex();
            instance.IndexCount = submesh.IndexCount;
            instance.VertexOffset = submesh.Geometry.VertexOffset();
            instance.Material = m_Materials.IndexOf(submesh.Material);
            instance.EntityId = entityId;

            const uint32_t batch = BatchFor(mesh, submeshIndex);
            m_Batches[batch].InstanceCount++;
            m_CullData[place].Batch = batch;
            UpdateSphere(place);

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
            Batch& batch = m_Batches[m_CullData[place].Batch];
            if (--batch.InstanceCount == 0)
            {
                // Lets go of the mesh. The batch's place in the list is used again by the next new batch.
                m_BatchIndices.erase(BatchKey{ batch.Mesh.get(), batch.SubmeshIndex });
                m_FreeBatches.push_back(m_CullData[place].Batch);
                batch = Batch();
                m_DrawOrderIsStale = true;
            }

            // Nothing is written to the GPU: a place without a batch is in no list of drawn instances.
            m_CullData[place].Batch = NoBatch;
        }

        m_FreeRuns[range.Count].push_back(range.First);
        m_InstanceCount -= range.Count;
    }

    void GpuScene::SetTransform(Range range, const glm::mat4& transform)
    {
        for (uint32_t place = range.First; place < range.First + range.Count; ++place)
        {
            m_Instances[place].Model = transform;
            UpdateSphere(place);
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
        m_CullData.resize(m_Instances.size());
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

    void GpuScene::UpdateSphere(uint32_t place)
    {
        const GpuInstance& instance = m_Instances[place];
        const glm::mat4& model = instance.Model;

        // The sphere grows by the largest of the three scales, which keeps it around the mesh however
        // unevenly the instance is scaled.
        const float scale = std::sqrt(std::max({ glm::dot(model[0], model[0]), glm::dot(model[1], model[1]), glm::dot(model[2], model[2]) }));
        const glm::vec3 centre = glm::vec3(model * glm::vec4(glm::vec3(instance.BoundingSphere), 1.0f));
        m_CullData[place].Sphere = glm::vec4(centre, instance.BoundingSphere.w * scale);
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
        Frame frame;
        frame.InstancesTested = m_InstanceCount;

        // A material is looked at once per frame: this is where a change to it is noticed and written to its table.
        for (Batch& batch : m_Batches)
        {
            batch.SeenByCamera.clear();
            batch.SeenBySun.clear();
            if (!batch.Mesh)
                continue;

            const std::shared_ptr<Material>& material = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex].Material;
            m_Materials.IndexOf(material);
            batch.HasCutOuts = material->Mode == AlphaMode::Mask;
            batch.IsTranslucent = material->Mode == AlphaMode::Translucent;
        }

        // Every instance is tested against both views. An instance is drawn in the passes whose view sees it.
        for (uint32_t place = 0; place < m_CullData.size(); ++place)
        {
            const CullData& instance = m_CullData[place];
            if (instance.Batch == NoBatch)
                continue;

            Batch& batch = m_Batches[instance.Batch];
            const glm::vec3 centre(instance.Sphere);
            if (camera.IsOnFrustum(centre, instance.Sphere.w))
                batch.SeenByCamera.push_back(place);

            // See-through surfaces cast no shadow.
            if (!batch.IsTranslucent && sun.IsOnFrustum(centre, instance.Sphere.w))
                batch.SeenBySun.push_back(place);
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

        // One draw command per batch and view that sees any of it. The command names a run in the list of
        // drawn instances, and the list names their places in the instance buffer.
        m_Visible.clear();
        std::vector<vk::DrawIndexedIndirectCommand> commands;
        const auto addCommands = [&](bool forCamera, bool withCutOuts)
        {
            DrawRange range;
            range.First = static_cast<uint32_t>(commands.size());

            for (const uint32_t index : m_DrawOrder)
            {
                const Batch& batch = m_Batches[index];
                const std::vector<uint32_t>& seen = forCamera ? batch.SeenByCamera : batch.SeenBySun;
                if (seen.empty() || batch.IsTranslucent || batch.HasCutOuts != withCutOuts)
                    continue;

                const Submesh& submesh = batch.Mesh->GetSubmeshes()[batch.SubmeshIndex];
                commands.emplace_back(submesh.IndexCount, static_cast<uint32_t>(seen.size()), submesh.Geometry.FirstIndex(), submesh.Geometry.VertexOffset(), static_cast<uint32_t>(m_Visible.size()));
                m_Visible.insert(m_Visible.end(), seen.begin(), seen.end());

                uint64_t& instances = forCamera ? frame.InstancesDrawn : frame.ShadowInstancesDrawn;
                uint64_t& triangles = forCamera ? frame.TrianglesDrawn : frame.ShadowTrianglesDrawn;
                instances += seen.size();
                triangles += uint64_t(submesh.IndexCount / 3) * seen.size();
            }

            range.Count = static_cast<uint32_t>(commands.size()) - range.First;
            return range;
        };

        frame.Draws.CameraOpaque = addCommands(true, false);
        frame.Draws.CameraCutOut = addCommands(true, true);
        frame.Draws.ShadowOpaque = addCommands(false, false);
        frame.Draws.ShadowCutOut = addCommands(false, true);
        frame.DrawCommands = commands.size();

        // See-through instances are drawn one by one, the farthest first, so that nearer ones cover them.
        struct Sorted
        {
            float Distance;
            uint32_t Place;
        };
        std::vector<Sorted> sorted;
        for (const uint32_t index : m_DrawOrder)
        {
            const Batch& batch = m_Batches[index];
            if (!batch.IsTranslucent)
                continue;

            for (const uint32_t place : batch.SeenByCamera)
                sorted.push_back({ glm::distance(cameraPosition, glm::vec3(m_Instances[place].Model[3])), place });
        }
        std::ranges::stable_sort(sorted, [](const Sorted& a, const Sorted& b) { return a.Distance > b.Distance; });

        m_TransparentDraws.clear();
        for (const Sorted& entry : sorted)
        {
            const GpuInstance& instance = m_Instances[entry.Place];
            m_TransparentDraws.push_back({ instance.IndexCount, instance.FirstIndex, instance.VertexOffset, static_cast<uint32_t>(m_Visible.size()) });
            m_Visible.push_back(entry.Place);

            frame.InstancesDrawn++;
            frame.TrianglesDrawn += instance.IndexCount / 3;
        }
        frame.TransparentDraws = m_TransparentDraws;

        UploadChanges();
        UploadList<uint32_t>(m_Context, m_Uploads, m_VisibleBuffer, m_Visible, vk::BufferUsageFlagBits::eStorageBuffer, "VisibleInstances");
        UploadList<vk::DrawIndexedIndirectCommand>(m_Context, m_Uploads, m_CommandBuffer, commands, vk::BufferUsageFlagBits::eIndirectBuffer, "MeshDrawCommands");

        frame.Draws.Buffer = &m_CommandBuffer;
        frame.InstanceAddress = m_InstanceBuffer.Address();
        frame.VisibleAddress = m_VisibleBuffer.Address();
        return frame;
    }
}
