#pragma once

#include "Frustum.h"
#include "RenderPass.h"
#include "Vulkan/DeletionQueue.h"
#include "Vulkan/GpuBuffer.h"
#include "Lynx/Scene/Scatter.h"

#include <glm/glm.hpp>

#include <array>
#include <map>
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

    // Where an instance is in the world: the first three rows of its matrix. The fourth is always 0, 0, 0, 1.
    // Has to match MeshTransform in Mesh.slang.
    struct InstanceTransform
    {
        glm::vec4 Rows[3];

        static InstanceTransform From(const glm::mat4& matrix)
        {
            const glm::mat4 rows = glm::transpose(matrix);
            return { { rows[0], rows[1], rows[2] } };
        }

        glm::vec3 Translation() const { return { Rows[0].w, Rows[1].w, Rows[2].w }; }
    };

    // One submesh of one mesh in the scene, as shaders read it. Has to match MeshInstance in Mesh.slang.
    // Besides what a vertex shader needs, it holds what culling on the GPU needs: the sphere around it
    // and the batch whose draw commands it is drawn with. Where it is in the world is kept apart, see
    // InstanceTransform: that is all that changes when it moves.
    struct GpuInstance
    {
        // In the mesh's own space: the centre in xyz, the radius in w.
        glm::vec4 BoundingSphere;

        uint32_t FirstIndex;
        uint32_t IndexCount;
        int32_t VertexOffset;

        // The place of the instance's material in the material table.
        uint32_t Material;

        // Written to the picking buffer in the editor. -1 for none.
        int32_t EntityId;

        // The instances of one submesh in one material form a batch. GpuScene::NoBatch for a place that
        // holds no instance, and for one that holds what scattered instances of a submesh have in common.
        uint32_t Batch;

        // For foliage, the address of what its vertices know about their leaves, see LeafVertex. Zero otherwise.
        uint64_t LeafData;
    };

    // What culling needs to know of the camera.
    struct CullCamera
    {
        Frustum Planes;
        glm::mat4 View;

        // The camera's own projection, as GLM makes it.
        glm::mat4 Projection;

        // The height of the picture in pixels, which says how large a distance in the world is on screen.
        float ViewportHeight = 0.0f;
    };

    // The meshes of a scene, kept on the GPU from frame to frame.
    //
    // Every submesh of every mesh in the scene is an instance with a place in one buffer. An instance is
    // written when it is added and again only when it changes, so a frame costs what changed in it and not
    // what the scene holds. What a frame draws is a list of places in that buffer, and a draw command
    // covers a run of it.
    //
    // Which instances are drawn, and in which level of detail, is decided on the GPU (CullPass). The CPU
    // only lays the frame out: one draw command per batch, level and view, each with a run of the list
    // that could hold all of the batch's instances. Culling counts the instances a view sees into the
    // command of their level and writes their places into its run.
    // The camera's view has two commands per batch and level, one for each phase of culling, see CullPhase.
    // See-through instances are the exception: they have to be sorted, which is done here, on the CPU.
    //
    // Scattered instances (ScatterInstance) are a second source of instances for the same batches. One
    // is 16 bytes in a buffer of its own, however many submeshes its mesh has, where an instance with a
    // place is 144 bytes per submesh. What the scattered instances of a submesh share, its geometry and
    // material, is held once, by an instance with a place of its own that is never drawn itself. Culling
    // counts both sources into the same draw commands, and an entry of the list of drawn instances says
    // which source it is from, so the passes and their draw calls know nothing of the difference.
    class GpuScene
    {
    public:
        static constexpr uint32_t NoBatch = ~0u;

        // An entry of the list of drawn instances names the place that holds what scattered instances
        // share in 24 bits, of which zero says that the entry is not a scattered instance.
        static constexpr uint32_t MaxPlaces = (1u << 24) - 1;

        // One drawn instance. Has to match the entries Mesh.slang reads.
        struct DrawnEntry
        {
            // The place of the instance, or where a scattered one is in the list of all of them.
            uint32_t Index = 0;

            // In the lowest byte the level of detail it is drawn in, in sixteenths. Above it, for a
            // scattered instance, the place that holds what it shares with the others, plus one.
            uint32_t LevelAndShared = 0;
        };

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

            // Where every instance is in the world, one InstanceTransform per place, and where it was
            // in the frame before.
            uint64_t TransformAddress = 0;
            uint64_t PreviousTransformAddress = 0;

            // Scattered instances count once each, whatever the number of their submeshes.
            uint64_t InstancesTested = 0;
            uint64_t ScatterInstances = 0;

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
        // A material in the list takes the place of the one the mesh has for the submesh of the same
        // number. A null entry, and a submesh past the end of the list, keeps the mesh's own.
        Range Add(const std::shared_ptr<StaticMesh>& mesh, std::span<const std::shared_ptr<Material>> materials, const glm::mat4& transform, int entityId);
        void Remove(Range range);
        void SetTransform(Range range, const glm::mat4& transform);

        // Adds scattered instances. An instance names its kind by its number in the list of meshes, and
        // is drawn with that mesh's own materials. An instance whose kind is not in the list, or whose
        // mesh is null, is left out, and so are see-through submeshes. The meshes have to be loaded.
        // Returns what to hand to RemoveScatter.
        //
        // Every call, and every removal, lays all scattered instances out anew: fine for a few sets
        // that change rarely, not for many that come and go.
        uint32_t AddScatter(std::span<const std::shared_ptr<StaticMesh>> kinds, std::span<const ScatterInstance> instances);
        void RemoveScatter(uint32_t scatter);

        // The address shaders read the scattered instances from, one ScatterInstance each, and zero if
        // there are none. Lays them out first if they changed, so it is right for the frame to come.
        uint64_t ScatterInstanceAddress();

        // Lays out the frame's draw commands and queues for upload what changed since the last frame.
        // Call once per frame, before the uploads are recorded.
        // sun holds the cascades of the sun's shadow.
        Frame Prepare(const CullCamera& camera, std::span<const ShadowCascade, ShadowCascadeCount> sun, const ShadowSettings& shadows, const glm::vec3& cameraPosition, const LodSettings& lods);

        // Copies the draw commands to where the CPU can read them, which is how it learns what culling
        // let through. Call once per frame, after the frame's passes are recorded.
        void CopyCommandsForCounters(vk::CommandBuffer commands);

    private:
        // The instances of one submesh in one material: what one draw command per view covers.
        // The material is part of it because its kind decides which pass draws the instances and how.
        struct Batch
        {
            std::shared_ptr<StaticMesh> Mesh;
            uint32_t SubmeshIndex = 0;
            std::shared_ptr<Lynx::Material> Material;

            // The places of the batch's instances, in no particular order.
            std::vector<uint32_t> Places;

            // Renewed every frame: the editor can change a material's kind while it is on screen.
            MeshDrawKind Kind;
            bool IsTranslucent = false;

            // How many scattered instances are drawn with the batch, and whether a kind of them uses it:
            // a batch that does stays, with or without places.
            uint32_t ScatterInstances = 0;
            bool IsScattered = false;
        };

        // The scattered instances of one call to AddScatter, as they were handed in.
        struct ScatterSet
        {
            std::vector<std::shared_ptr<StaticMesh>> Kinds;
            std::vector<ScatterInstance> Instances;
        };

        // One submesh of a kind of scattered instance: the batch it is drawn with and the place that
        // holds its geometry and material. Has to match ScatterBatch in Cull.slang.
        struct ScatterBatch
        {
            uint32_t Batch = 0;
            uint32_t SharedPlace = 0;
        };

        // A kind of scattered instance: a mesh, and where the list of scatter batches has its submeshes.
        struct ScatterKind
        {
            std::shared_ptr<StaticMesh> Mesh;
            uint32_t FirstBatch = 0;
            uint32_t BatchCount = 0;
            uint32_t InstanceCount = 0;
        };

        struct BatchKey
        {
            const StaticMesh* Mesh;
            uint32_t SubmeshIndex;
            const Lynx::Material* Material;

            bool operator==(const BatchKey&) const = default;
        };

        struct BatchKeyHash
        {
            size_t operator()(const BatchKey& key) const
            {
                // Many batches differ in the material alone, so it has to reach every bit of the result.
                const size_t meshAndSubmesh = std::hash<const void*>()(key.Mesh) ^ (std::hash<uint32_t>()(key.SubmeshIndex) << 1);
                return meshAndSubmesh ^ (std::hash<const void*>()(key.Material) * 0x9E3779B97F4A7C15ull);
            }
        };

        // The draw commands of one frame as the GPU left them, and what is needed to count from them.
        struct CommandCopy
        {
            GpuBuffer Commands;
            uint32_t CommandCount = 0;

            // The commands of the camera's view come first, of both phases, those of the sun's after them.
            uint32_t CameraCommandCount = 0;

            // Culled and counted on the CPU.
            uint64_t SeeThroughInstances = 0;
            uint64_t SeeThroughTriangles = 0;
        };

        // A copy is read when its frame has left the GPU for certain, which is as long as a buffer
        // that was given up is kept.
        static constexpr size_t CommandCopyCount = DeletionQueue::FramesToKeep + 1;

        uint32_t TakePlaces(uint32_t count);
        uint32_t BatchFor(const std::shared_ptr<StaticMesh>& mesh, uint32_t submeshIndex, const std::shared_ptr<Material>& material);
        void ReleaseBatchIfUnused(uint32_t batchIndex);
        void LayOutScatter();
        uint32_t ScatterKindFor(const std::shared_ptr<StaticMesh>& mesh);
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

        // Where every instance is, per place, and the same on the GPU, twice: one buffer holds this
        // frame's transforms and the other the last frame's, and they change roles every frame. So an
        // instance that moves costs one transform a frame, and what did not move costs nothing.
        std::vector<InstanceTransform> m_Transforms;
        std::array<GpuBuffer, 2> m_TransformBuffers;

        // Places whose instance moved, in this frame and in the last. The buffer for this frame was
        // last written two frames ago, so it is brought up to date with both.
        std::vector<uint32_t> m_Moved;
        std::vector<uint32_t> m_MovedLastFrame;
        std::vector<uint32_t> m_MovedInEitherFrame;

        // Places that got a new instance. It did not move to where it is, so both buffers get its transform.
        std::vector<uint32_t> m_Placed;

        // Per place, whether the camera saw its instance in the last frame. Only the GPU reads and writes it.
        GpuBuffer m_VisibilityBuffer;

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

        // What culling reads besides the instances: the views of the frame, then per batch its commands.
        GpuBuffer m_CullInputBuffer;

        std::vector<TransparentDraw> m_TransparentDraws;

        // What was handed in, by the number AddScatter returned, in the order they are laid out in.
        std::map<uint32_t, ScatterSet> m_ScatterSets;
        uint32_t m_NextScatter = 0;
        bool m_ScatterIsStale = false;

        // Laid out from the sets: every kind there is, their submeshes, and all instances in one list
        // with the kinds numbered as in the list of kinds here.
        std::vector<ScatterKind> m_ScatterKinds;
        std::vector<ScatterBatch> m_ScatterBatches;
        std::vector<ScatterInstance> m_ScatterInstances;
        GpuBuffer m_ScatterInstanceBuffer;

        // Per scattered instance, one bit per submesh: whether the camera saw it in the last frame.
        // Only the GPU reads and writes it.
        GpuBuffer m_ScatterVisibilityBuffer;

        std::array<CommandCopy, CommandCopyCount> m_CommandCopies;
        uint64_t m_Frame = 0;
        uint32_t m_InstanceCount = 0;
    };
}
