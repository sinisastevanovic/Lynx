#pragma once

#include "Frustum.h"
#include "RenderPass.h"
#include "Vulkan/DeletionQueue.h"
#include "Vulkan/GpuBuffer.h"

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace Lynx
{
    class GraphicsContext;
    class Material;
    class MaterialTable;
    class StaticMesh;
    class UploadQueue;

    // One submesh of one mesh in the scene, as shaders read it. Has to match MeshInstance in Mesh.slang.
    // Besides what a vertex shader needs, it holds what culling on the GPU needs: the sphere around it
    // and the batch whose draw commands it is drawn with.
    struct GpuInstance
    {
        glm::mat4 Model;

        // In the mesh's own space: the centre in xyz, the radius in w.
        glm::vec4 BoundingSphere;

        uint32_t FirstIndex;
        uint32_t IndexCount;
        int32_t VertexOffset;

        // The place of the instance's material in the material table.
        uint32_t Material;

        // Written to the picking buffer in the editor. -1 for none.
        int32_t EntityId;

        // The instances of one submesh form a batch. GpuScene::NoBatch for a place that holds no instance.
        uint32_t Batch;
        uint32_t Padding[2];
    };

    // The meshes of a scene, kept on the GPU from frame to frame.
    //
    // Every submesh of every mesh in the scene is an instance with a place in one buffer. An instance is
    // written when it is added and again only when it changes, so a frame costs what changed in it and not
    // what the scene holds. What a frame draws is a list of places in that buffer, and a draw command
    // covers a run of it.
    //
    // Which instances are drawn is decided on the GPU (CullPass). The CPU only lays the frame out: one
    // draw command per batch and view, each with a run of the list that could hold all of the batch's
    // instances. Culling counts the instances a view sees into the command and writes their places into its run.
    // See-through instances are the exception: they have to be sorted, which is done here, on the CPU.
    class GpuScene
    {
    public:
        static constexpr uint32_t NoBatch = ~0u;

        // The instances of one mesh in the scene: one per submesh that can be drawn, next to each other.
        struct Range
        {
            uint32_t First = 0;
            uint32_t Count = 0;
        };

        // What the mesh passes of one frame draw from.
        struct Frame
        {
            MeshDrawCommands Draws;
            MeshCulling Culling;
            std::span<const TransparentDraw> TransparentDraws;

            // The addresses shaders read the instances from, and the list of the places of those that are drawn.
            uint64_t InstanceAddress = 0;
            uint64_t VisibleAddress = 0;

            uint64_t InstancesTested = 0;

            // What the GPU decided, known a few frames late: these are of the frame drawn that long ago.
            // An instance counts once per view it is drawn in.
            uint64_t InstancesDrawn = 0;
            uint64_t TrianglesDrawn = 0;
            uint64_t ShadowInstancesDrawn = 0;
            uint64_t ShadowTrianglesDrawn = 0;
            uint64_t DrawCommands = 0;
        };

        GpuScene(GraphicsContext& context, UploadQueue& uploads, MaterialTable& materials);
        ~GpuScene();

        // Removes every instance, for when another scene takes the place of the one that was drawn.
        void Clear();

        // The mesh has to be loaded. A submesh without a material or without geometry gets no instance,
        // so the range can be shorter than the mesh's list of submeshes, or empty.
        Range Add(const std::shared_ptr<StaticMesh>& mesh, const glm::mat4& transform, int entityId);
        void Remove(Range range);
        void SetTransform(Range range, const glm::mat4& transform);

        // Lays out the frame's draw commands and queues for upload what changed since the last frame.
        // Call once per frame, before the uploads are recorded.
        Frame Prepare(const Frustum& camera, const Frustum& sun, const glm::vec3& cameraPosition);

        // Copies the draw commands to where the CPU can read them, which is how it learns what culling
        // let through. Call once per frame, after the frame's passes are recorded.
        void CopyCommandsForCounters(vk::CommandBuffer commands);

    private:
        // The instances of one submesh: what one draw command per view covers.
        struct Batch
        {
            std::shared_ptr<StaticMesh> Mesh;
            uint32_t SubmeshIndex = 0;

            // The places of the batch's instances, in no particular order.
            std::vector<uint32_t> Places;

            // Renewed every frame: the editor can change a material's kind while it is on screen.
            bool HasCutOuts = false;
            bool IsTranslucent = false;
        };

        struct BatchKey
        {
            const StaticMesh* Mesh;
            uint32_t SubmeshIndex;

            bool operator==(const BatchKey&) const = default;
        };

        struct BatchKeyHash
        {
            size_t operator()(const BatchKey& key) const { return std::hash<const void*>()(key.Mesh) ^ (std::hash<uint32_t>()(key.SubmeshIndex) << 1); }
        };

        // The draw commands of one frame as the GPU left them, and what is needed to count from them.
        struct CommandCopy
        {
            GpuBuffer Commands;
            uint32_t CommandCount = 0;

            // The commands of the camera's view come first, those of the sun's after them.
            uint32_t CameraCommandCount = 0;

            // Culled and counted on the CPU.
            uint64_t SeeThroughInstances = 0;
            uint64_t SeeThroughTriangles = 0;
        };

        // A copy is read when its frame has left the GPU for certain, which is as long as a buffer
        // that was given up is kept.
        static constexpr size_t CommandCopyCount = DeletionQueue::FramesToKeep + 1;

        uint32_t TakePlaces(uint32_t count);
        uint32_t BatchFor(const std::shared_ptr<StaticMesh>& mesh, uint32_t submeshIndex);
        void UploadChanges();
        void CullSeeThrough(const Frustum& camera, const glm::vec3& cameraPosition, uint32_t firstVisible, CommandCopy& counted);
        void ReadCounters(const CommandCopy& copy, Frame& frame) const;

    private:
        GraphicsContext& m_Context;
        UploadQueue& m_Uploads;
        MaterialTable& m_Materials;

        // What the instance buffer holds, kept to fill a larger buffer from.
        std::vector<GpuInstance> m_Instances;
        GpuBuffer m_InstanceBuffer;

        // Per place, where the place stands in the list of its batch.
        std::vector<uint32_t> m_IndexInBatch;

        // Places whose instance has to be written to the GPU again.
        std::vector<uint32_t> m_Changed;

        // Free places, by the length of the run they form.
        std::unordered_map<uint32_t, std::vector<uint32_t>> m_FreeRuns;

        std::vector<Batch> m_Batches;
        std::vector<uint32_t> m_FreeBatches;
        std::unordered_map<BatchKey, uint32_t, BatchKeyHash> m_BatchIndices;

        // The batches in the order they are drawn in, which has to be the same in every run. Rebuilt when batches come or go.
        std::vector<uint32_t> m_DrawOrder;
        bool m_DrawOrderIsStale = true;

        // Rewritten every frame: by the CPU with the layout, then by culling on the GPU.
        GpuBuffer m_VisibleBuffer;
        GpuBuffer m_CommandBuffer;

        // What culling reads besides the instances: the planes of the two views, then per batch its commands.
        GpuBuffer m_CullInputBuffer;

        std::vector<TransparentDraw> m_TransparentDraws;

        std::array<CommandCopy, CommandCopyCount> m_CommandCopies;
        uint64_t m_Frame = 0;
        uint32_t m_InstanceCount = 0;
    };
}
