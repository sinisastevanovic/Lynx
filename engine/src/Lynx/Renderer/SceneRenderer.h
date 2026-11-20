#pragma once

#include "ExtractedScene.h"

#include <glm/glm.hpp>

#include <memory>

namespace Lynx
{
    class EditorCamera;
    class FrameProfiler;
    class Renderer;
    class Scene;

    // Copies what a frame of the scene looks like out of the scene and hands it to the renderer.
    // This is the only place where rendering reads the scene.
    class SceneRenderer
    {
    public:
        SceneRenderer(Renderer& renderer, FrameProfiler& profiler, std::shared_ptr<Scene> scene);

        void SetScene(std::shared_ptr<Scene> scene);
        void SetViewportSize(uint32_t width, uint32_t height);

        // Both have to be called inside a frame of the renderer.
        void RenderEditor(EditorCamera& camera, float deltaTime);

        // Draws from the scene's primary camera, and nothing if it has none.
        // physicsAlpha is how far the frame lies between the last two physics steps, from 0 to 1.
        void RenderRuntime(float deltaTime, float physicsAlpha);

    private:
        void Extract(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float deltaTime, bool isEditor, float physicsAlpha);
        void ExtractMeshes(bool isEditor, float physicsAlpha);
        void DrawColliders();

    private:
        Renderer& m_Renderer;
        FrameProfiler& m_Profiler;
        std::shared_ptr<Scene> m_Scene;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;
        bool m_ViewportDirty = true;

        // Reused every frame for its memory.
        ExtractedScene m_Extracted;
    };
}
