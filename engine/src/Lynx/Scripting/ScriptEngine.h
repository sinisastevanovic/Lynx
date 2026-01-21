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

        void OnEditorStart(Scene* scene);
        void OnEditorEnd();
        void OnRuntimeStart(Scene* scene);
        void OnRuntimeStop();

        void OnUpdateEntity(Entity entity, float deltaTime);
        void OnActionEvent(const std::string& action, bool pressed);
        
        void OnScriptComponentAdded(Entity entity);
        void OnScriptComponentDestroyed(Entity entity);

        void ReloadScript(AssetHandle handle);

    private:
        bool InstantiateScript(Entity entity, ScriptInstance& instance);
        void InitializeProperties(ScriptInstance& instance);
        
        template<typename... Args>
        void CallMethod(ScriptInstance& instance, const std::string& methodName, Args&&... args);

        struct ScriptEngineData;
        ScriptEngineData* m_Data = nullptr;
        
        friend class Engine;
    };
}