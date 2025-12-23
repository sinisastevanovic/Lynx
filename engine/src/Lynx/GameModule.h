#pragma once

namespace Lynx
{
    class ComponentRegistry;

    class LX_API IGameModule 
    {
    public:
        virtual ~IGameModule() = default;

        virtual void RegisterScripts() = 0;
        virtual void RegisterComponents(ComponentRegistry* registry) = 0;
        virtual void OnStart() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnShutdown() = 0;
    };

    extern "C" {
        typedef IGameModule* (*CreateGameFunc)();
        typedef void (*DestroyGameFunc)(IGameModule*);
    }
}