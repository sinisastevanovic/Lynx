#pragma once

#include "ExtractedScene.h"

#include <glm/glm.hpp>

#include <entt/entt.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Lynx
{
    class EditorCamera;
    class FrameProfiler;
    class Material;
    class Renderer;
    class Scene;
    class StaticMesh;

    // Brings a scene to the renderer. Its meshes are kept in step with the renderer's copy of them on the
    // GPU: only what was added, removed or moved since the last frame is passed on. Everything else is
    // copied out of the scene anew for every frame. This is the only place where rendering reads the scene.
    class SceneRenderer
    {
    public:
        SceneRenderer(Renderer& renderer, FrameProfiler& profiler, std::shared_ptr<Scene> scene);
        ~SceneRenderer();

        void SetScene(std::shared_ptr<Scene> scene);
        void SetViewportSize(uint32_t width, uint32_t height);

        // How the scene is shown: the lit picture, or one thing about it. Applies from the next frame.
        ViewSettings& GetViewSettings() { return m_Viewing; }

        // Both have to be called inside a frame of the renderer.
        void RenderEditor(EditorCamera& camera, float deltaTime);

        // Draws from the scene's primary camera, and nothing if it has none.
        // physicsAlpha is how far the frame lies between the last two physics steps, from 0 to 1.
        void RenderRuntime(float deltaTime, float physicsAlpha);

    private:
        void Extract(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float deltaTime, bool isEditor, float physicsAlpha);
        void DrawColliders(const glm::vec3& cameraPosition);

        // Following a scene: the renderer learns of mesh entities that come, go or change as it happens.
        void Follow(Scene& scene);
        void Unfollow(Scene& scene);
        void OnMeshAdded(entt::registry& registry, entt::entity entity);
        void OnMeshRemoved(entt::registry& registry, entt::entity entity);
        void OnEntityEnabled(entt::registry& registry, entt::entity entity);
        void OnEntityDisabled(entt::registry& registry, entt::entity entity);
        void OnInstancesRemoved(entt::registry& registry, entt::entity entity);
        void OnScatterChanged(entt::registry& registry, entt::entity entity);
        void OnScatterRemoved(entt::registry& registry, entt::entity entity);
        void OnDrawnScatterRemoved(entt::registry& registry, entt::entity entity);

        void SyncMeshes(bool isEditor, float physicsAlpha);
        void SyncScatter();
        void AddInstances(entt::entity entity, bool isEditor, float physicsAlpha);

    private:
        Renderer& m_Renderer;
        FrameProfiler& m_Profiler;
        std::shared_ptr<Scene> m_Scene;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;
        bool m_ViewportDirty = true;
        ViewSettings m_Viewing;

        // Reused every frame for its memory.
        ExtractedScene m_Extracted;

        // Reused for its memory: the materials an entity gives its mesh, while its instances are made.
        std::vector<std::shared_ptr<Material>> m_GivenMaterials;

        // Entities that may need instances: new mesh entities, and ones whose mesh or materials were replaced.
        // Looked at when the frame is drawn, by which time whoever made them has finished setting them up.
        std::vector<entt::entity> m_PendingEntities;

        // Entities whose scattered instances are new or were replaced.
        std::vector<entt::entity> m_PendingScatter;

        // Every mesh that has instances, with the entities that use it. When a mesh is loaded or
        // reloaded it gets a new version, and the instances of its users are made anew.
        struct MeshUse
        {
            std::shared_ptr<StaticMesh> Mesh;
            uint32_t Version = 0;
            std::vector<entt::entity> Users;
        };
        std::unordered_map<const StaticMesh*, MeshUse> m_MeshUses;
    };
}
