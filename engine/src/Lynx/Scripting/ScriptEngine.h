#pragma once

#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Entity.h"

namespace Lynx
{
    class LX_API ScriptEngine
    {
    public:
        ScriptEngine();
        ~ScriptEngine();

        void Init();
        void Shutdown();

        void OnRuntimeStart(Scene* scene);
        void OnRuntimeStop();

        void OnUpdateEntity(Entity entity, float deltaTime);
        void OnCreateEntity(Entity entity);

    private:
        void LoadScript(const std::string& scriptPath);

        struct ScriptEngineData;
        ScriptEngineData* m_Data = nullptr;
    };
}