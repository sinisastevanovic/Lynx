#pragma once

#include <sol/sol.hpp>

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
        
        template<typename... Args>
        void OnGlobalEvent(Scene* scene, const std::string& functionName, Args&&... args)
        {
            DispatchGlobalEvent(scene, functionName, [&](sol::protected_function func, sol::table self)
            {
                auto result = func(self, std::forward<Args>(args)...);
                if (!result.valid())
                {
                    sol::error err = result;
                    LX_CORE_ERROR("Lua Global Event Runtime Error ({0}): {1}", functionName, err.what());
                }
            });
        }
        
    private:
        void InitializeProperties(ScriptInstance& instance);
        void ResolveScriptProperties(ScriptInstance& instance, Scene* context);
        
        void DispatchGlobalEvent(Scene* scene, const std::string& name, std::function<void(sol::protected_function, sol::table)> caller);
        
        template<typename... Args>
        void CallMethod(ScriptInstance& instance, const std::string& methodName, Args&&... args);

        struct ScriptEngineData;
        ScriptEngineData* m_Data = nullptr;
    };
}