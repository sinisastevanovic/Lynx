#pragma once
#include "ComponentRegistry.h"
#include "Window.h"
#include "Event/Event.h"

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
        bool m_IsRunning = true;
    };
}

