#pragma once
#include "ComponentRegistry.h"
#include "Window.h"
#include "Renderer/Renderer.h"
#include "Event/Event.h"
#include "Lynx/Renderer/EditorCamera.h"

namespace Lynx
{
    class IGameModule;

    class LX_API Engine
    {
    public:
        ComponentRegistry ComponentRegistry;
    
        void Initialize();
        void Run(IGameModule* gameModule);
        void Shutdown();

        void OnEvent(Event& event);

    private:
        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Renderer> m_Renderer;
        bool m_IsRunning = true;
        EditorCamera m_EditorCamera;
    };
}

