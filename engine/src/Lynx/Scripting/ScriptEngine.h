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

        void OnEditorStart(Scene* scene, bool recreateScripts = false);
        void OnEditorEnd();
        void OnRuntimeStart(Scene* scene);
        void OnRuntimeStop();

        void OnUpdateEntity(Entity entity, float deltaTime);
        void OnCreateEntity(Entity entity);
        void OnDestroyEntity(Entity entity);

        void ReloadScript(AssetHandle handle);

        void LoadScript(Entity entity);

        void OnActionEvent(const std::string& action, bool pressed);
    private:

        struct ScriptEngineData;
        ScriptEngineData* m_Data = nullptr;
    };
}