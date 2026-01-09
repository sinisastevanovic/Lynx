#pragma once
#include "EditorCamera.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    class SceneRenderer
    {
    public:
        SceneRenderer(std::shared_ptr<Scene> scene);
        
        void SetScene(std::shared_ptr<Scene> scene);
        void SetViewportSize(uint32_t width, uint32_t height);
        
        void RenderEditor(EditorCamera& camera, float deltaTime);
        void RenderRuntime(float deltaTime);
        
        // TODO: We should have some kind of editor view options structure
        void SetShowColliders(bool show) { m_ShowColliders = show; }
        bool GetShowColliders() const { return m_ShowColliders; }
        
    private:
        void SubmitScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float deltaTime, bool isEditor);
        void SetViewportDirty(bool dirty) { m_ViewportDirty = true; }
        
    private:
        std::shared_ptr<Scene> m_Scene;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;
        bool m_ShowColliders = false;
        bool m_ViewportDirty = true;
        
        friend class Engine;
    };
}

