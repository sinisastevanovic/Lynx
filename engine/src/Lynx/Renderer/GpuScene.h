#pragma once

#include "Frustum.h"
#include "RenderPass.h"
#include "Vulkan/GpuBuffer.h"

#include <glm/glm.hpp>

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
    // Besides what a vertex shader needs, it holds what culling on the GPU will need to write a draw
    // command for it: the sphere around it and its place in the geometry pool.
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
        uint32_t Padding[3];
    };

    // The meshes of a scene, kept on the GPU from frame to frame.
    //
    // Every submesh of every mesh in the scene is an instance with a place in one buffer. An instance is
    // written when it is added and again only when it changes, so a frame costs what changed in it and not
    // what the scene holds. What a frame draws is a list of places in that buffer: culling writes the
    // list, and a draw command covers a run of it.
    class GpuScene
    {
    public:
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
            std::span<const TransparentDraw> TransparentDraws;

            // The addresses shaders read the instances from, and the list of the places of those that are drawn.
            uint64_t InstanceAddress = 0;
            uint64_t VisibleAddress = 0;

            // An instance counts once per view it is drawn in.
            uint64_t InstancesTested = 0;
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

        // Decides which instances the camera and the sun see, and queues for upload what changed since the
        // last frame and the lists of what is drawn. Call once per frame, before the uploads are recorded.
        Frame Prepare(const Frustum& camera, const Frustum& sun, const glm::vec3& cameraPosition);

    private:
        // The instances of one submesh: what one draw command covers.
        struct Batch
        {
            std::shared_ptr<StaticMesh> Mesh;
            uint32_t SubmeshIndex = 0;
            uint32_t InstanceCount = 0;

            // Renewed every frame: the editor can change a material's kind while it is on screen.
            bool HasCutOuts = false;
            bool IsTranslucent = false;

            // The places of the instances the two views see in this frame.
            std::vector<uint32_t> SeenByCamera;
            std::vector<uint32_t> SeenBySun;
        };

        // What culling reads of an instance, kept apart from the rest so that testing all of them
        // touches as little memory as possible.
        struct CullData
        {
            // In the world: the centre in xyz, the radius in w.
            glm::vec4 Sphere{ 0.0f };

            // NoBatch for a place that holds no instance.
            uint32_t Batch = NoBatch;
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

        static constexpr uint32_t NoBatch = ~0u;

        uint32_t TakePlaces(uint32_t count);
        uint32_t BatchFor(const std::shared_ptr<StaticMesh>& mesh, uint32_t submeshIndex);
        void UpdateSphere(uint32_t place);
        void UploadChanges();

    private:
        GraphicsContext& m_Context;
        UploadQueue& m_Uploads;
        MaterialTable& m_Materials;

        // What the instance buffer holds, kept to cull from and to fill a larger buffer from.
        std::vector<GpuInstance> m_Instances;
        std::vector<CullData> m_CullData;
        GpuBuffer m_InstanceBuffer;

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

        // Rewritten every frame.
        GpuBuffer m_VisibleBuffer;
        GpuBuffer m_CommandBuffer;
        std::vector<uint32_t> m_Visible;
        std::vector<TransparentDraw> m_TransparentDraws;

        uint32_t m_InstanceCount = 0;
    };
}
