#pragma once


#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Entity.h"

namespace Lynx
{
    struct ScriptInstance;

    class LX_API ScriptEngine
    {
    public:
        ScriptEngine();
        ~ScriptEngine();

        void Init();
        void Shutdown();

        void OnRuntimeStart(Scene* scene);

        void OnUpdateEntity(Entity entity, float deltaTime);
        void OnActionEvent(const std::string& action, bool pressed);
        
        void OnScriptComponentAdded(Entity entity);
        void OnScriptComponentDestroyed(Entity entity);

        void ReloadScript(AssetHandle handle);

        bool InstantiateScript(Entity entity, ScriptInstance& instance);
    private:
        void InitializeProperties(ScriptInstance& instance);
        void ResolveScriptProperties(ScriptInstance& instance, Scene* context);
        
        template<typename... Args>
        void CallMethod(ScriptInstance& instance, const std::string& methodName, Args&&... args);

        struct ScriptEngineData;
        ScriptEngineData* m_Data = nullptr;
    };
}