#include "lxpch.h"
#include "ScriptEngine.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"
#include "Lynx/Scene/Components/Components.h"

#include <sol/sol.hpp>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Script.h"

namespace Lynx
{
    struct ScriptEngine::ScriptEngineData
    {
        sol::state Lua;
        Scene* SceneContext = nullptr;
    };
    
    ScriptEngine::ScriptEngine()
    {
        Init();
    }

    ScriptEngine::~ScriptEngine()
    {
        Shutdown();
    }

    void ScriptEngine::Init()
    {
        m_Data = new ScriptEngineData();
        m_Data->Lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table);

        m_Data->Lua.set_function("print", [](sol::variadic_args args)
        {
            std::string message;
            for (auto arg : args)
            {
                message += arg.as<std::string>() + "\t";
            }

            LX_CORE_INFO("[LUA] {0}", message);
        });

        m_Data->Lua.new_usertype<Entity>("Entity",
            sol::constructors<Entity(entt::entity, Scene*)>(),
            "GetID", &Entity::GetUUID,
            "Transform", sol::property(
                [](Entity& entity) -> TransformComponent&
                {
                    return entity.GetComponent<TransformComponent>();
                }
            )
        );

        m_Data->Lua.new_usertype<TransformComponent>("Transform",
            "Translation", &TransformComponent::Translation,
            "Scale", &TransformComponent::Scale,
            "Rotation", sol::property(
                [](TransformComponent& t) { return t.GetRotationDegrees(); },
                [](TransformComponent& t, const glm::vec3& degrees) { t.SetRotionDegrees(degrees); }
            )
        );

        m_Data->Lua.new_usertype<glm::vec3>("Vec3",
            sol::constructors<glm::vec3(float, float, float), glm::vec3()>(),
            "x", &glm::vec3::x,
            "y", &glm::vec3::y,
            "z", &glm::vec3::z,
            sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
            sol::meta_function::multiplication, [](const glm::vec3& a, const glm::vec3& b) { return a * b; }
        );

        LX_CORE_INFO("ScriptEngine Initialized");
    }

    void ScriptEngine::Shutdown()
    {
        if (m_Data)
        {
            delete m_Data;
            m_Data = nullptr;
        }
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        m_Data->SceneContext = scene;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        m_Data->SceneContext = nullptr;
    }

    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        if (!entity.HasComponent<LuaScriptComponent>())
            return;

        auto& lsc = entity.GetComponent<LuaScriptComponent>();
        if (!lsc.ScriptHandle.IsValid())
            return;

        // Needed, as this loads the asset and makes reloading possible
        auto scriptAsset = Engine::Get().GetAssetManager().GetAsset<Script>(lsc.ScriptHandle);
        std::filesystem::path scriptPath = scriptAsset->GetFilePath();
        if (scriptPath.empty())
        {
            LX_CORE_ERROR("Could not find script path for asset {0}", lsc.ScriptHandle);
            return;
        }

        sol::load_result result = m_Data->Lua.load_file(scriptPath.string());
        if (!result.valid())
        {
            sol::error err = result;
            LX_CORE_ERROR("Failed to load script '{0}': {1}", scriptPath.string(), err.what());
            return;
        }

        sol::protected_function_result scriptResult = result();
        if (scriptResult.valid())
        {
            lsc.Self = scriptResult.get<sol::table>();
            lsc.Self["CurrentEntity"] = entity;

            sol::protected_function onCreate = lsc.Self["OnCreate"];
            if (onCreate.valid())
            {
                sol::protected_function_result createResult = onCreate(lsc.Self);
                if (!createResult.valid())
                {
                    sol::error err = createResult;
                    LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                }
            }
        }
        else
        {
            sol::error err = scriptResult;
            LX_CORE_ERROR("Failed to execute script '{0}': {1}", scriptPath.string(), err.what());
        }
    }

    void ScriptEngine::OnDestroyEntity(Entity entity)
    {
        if (!entity.HasComponent<LuaScriptComponent>())
            return;

        auto& lsc = entity.GetComponent<LuaScriptComponent>();
        if (lsc.Self.valid())
        {
            sol::protected_function onDestroy = lsc.Self["OnDestroy"];
            if (onDestroy.valid())
            {
                sol::protected_function_result result = onDestroy(lsc.Self);
                if (!result.valid())
                {
                    sol::error err = result;
                    LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                }
            }

            lsc.Self = sol::nil;
        }
    }

    void ScriptEngine::ReloadScript(AssetHandle handle)
    {
        if (!m_Data->SceneContext)
            return;

        auto view = m_Data->SceneContext->Reg().view<LuaScriptComponent>();
        for (auto& entityId : view)
        {
            Entity entity { entityId, m_Data->SceneContext };
            auto& lsc = entity.GetComponent<LuaScriptComponent>();

            if (handle == lsc.ScriptHandle)
            {
                LX_CORE_INFO("Reloading script for Entity {0}", (uint64_t)entityId);

                std::string scriptPath = Engine::Get().GetAssetRegistry().Get(lsc.ScriptHandle).FilePath.string();

                if (lsc.Self.valid() && lsc.Self["OnDestroy"].valid())
                {
                    sol::protected_function_result result = lsc.Self["OnDestroy"](lsc.Self);
                    if (!result.valid())
                    {
                        sol::error err = result;
                        LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                    }
                }

                sol::load_result result = m_Data->Lua.load_file(scriptPath);
                if (!result.valid())
                {
                    sol::error err = result;
                    LX_CORE_ERROR("Failed to reload script: {0}", err.what());
                    continue;
                }

                sol::protected_function_result scriptResult = result();
                if (scriptResult.valid())
                {
                    lsc.Self = scriptResult.get<sol::table>();
                    lsc.Self["CurrentEntity"] = entity;

                    if (lsc.Self["OnCreate"].valid())
                    {
                        sol::protected_function_result createResult = lsc.Self["OnCreate"](lsc.Self);
                        if (!createResult.valid())
                        {
                            sol::error err = createResult;
                            LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                        }
                    }
                }
            }
        }
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, float deltaTime)
    {
        if (!entity.HasComponent<LuaScriptComponent>())
            return;

        auto& lsc = entity.GetComponent<LuaScriptComponent>();
        if (lsc.Self.valid())
        {
            sol::protected_function onUpdate = lsc.Self["OnUpdate"];
            if (onUpdate.valid())
            {
                sol::protected_function_result result = onUpdate(lsc.Self, deltaTime);
                if (!result.valid())
                {
                    sol::error err = result;
                    LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                }
            }
        }
    }

    void ScriptEngine::LoadScript(const std::string& scriptPath)
    {
    }
}
