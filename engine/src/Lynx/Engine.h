#pragma once
#include "ComponentRegistry.h"
#include "Window.h"
#include "Asset/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Event/Event.h"
#include "Lynx/Renderer/EditorCamera.h"

namespace Lynx
{
    class IGameModule;
    class EditorCamera;
    class Renderer;
    class Scene;

    class LX_API Engine
    {
    public:
        static Engine& Get() { return *s_Instance; }
        
        void Initialize();
        void Run(IGameModule* gameModule);
        void Shutdown();

        inline Window& GetWindow() { return *m_Window; }
        AssetManager& GetAssetManager() { return *m_AssetManager; }
        
        std::shared_ptr<Scene> GetActiveScene() const { return m_Scene; }

        ComponentRegistry ComponentRegistry;

    private:
        void OnEvent(Event& e);

    private:
        static Engine* s_Instance;
        
        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Renderer> m_Renderer;
        std::unique_ptr<AssetManager> m_AssetManager;
        std::shared_ptr<Scene> m_Scene;
        
        EditorCamera m_EditorCamera;
        
        bool m_IsRunning = true;
    };
}

