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
        bool InstantiateScript(Entity entity);
        void InitializeProperties(Entity entity);
        
        template<typename... Args>
        void CallMethod(Entity entity, const std::string& methodName, Args&&... args);

        struct ScriptEngineData;
        ScriptEngineData* m_Data = nullptr;
    };
}