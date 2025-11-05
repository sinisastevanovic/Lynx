#pragma once
#include "ComponentRegistry.h"
#include "Window.h"

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

    private:
        std::unique_ptr<Window> m_Window;
        bool m_IsRunning = true;
    };
}

