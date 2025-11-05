#pragma once
#include "ComponentRegistry.h"

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
    };
}

