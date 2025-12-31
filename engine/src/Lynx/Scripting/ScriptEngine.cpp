#include "lxpch.h"
#include "ScriptEngine.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"
#include "Lynx/Scene/Components/Components.h"

#include <sol/sol.hpp>

#include "ScriptEngineData.h"
#include "Lynx/Engine.h"
#include "Lynx/Input.h"
#include "Lynx/Asset/Script.h"
#include "Lynx/Renderer/DebugRenderer.h"

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

        ScriptEngineUtils::RegisterBasicTypes(m_Data->Lua);

        m_Data->Lua.new_usertype<Entity>("Entity",
            sol::constructors<Entity(entt::entity, Scene*)>(),
            "GetID", &Entity::GetUUID,
            "Transform", sol::property(
                [](Entity& entity) -> TransformComponent&
                {
                    return entity.GetComponent<TransformComponent>();
                }
            ),
            "MeshComponent", sol::property(
                [](Entity& entity) -> MeshComponent*
                {
                    if (entity.HasComponent<MeshComponent>())
                        return &entity.GetComponent<MeshComponent>();
                    return nullptr;
                }
            )
        );

        m_Data->Lua.new_usertype<TransformComponent>("Transform",
            "Translation", &TransformComponent::Translation,
            "Scale", &TransformComponent::Scale,
            "Rotation", sol::property(
                [](TransformComponent& t) { return t.GetRotationDegrees(); },
                [](TransformComponent& t, const glm::vec3& degrees) { t.SetRotionDegrees(degrees); }
            ),
            "RotationQuat", &TransformComponent::Rotation
        );

        m_Data->Lua.new_usertype<MeshComponent>("MeshComponent",
            "MeshHandle", &MeshComponent::Mesh
        );

        m_Data->Lua.new_usertype<Input>("Input",
            "GetButton", &Input::GetButton,
            "GetAxis", &Input::GetAxis
        );

        // --- DEBUG ---
        auto debug = m_Data->Lua.create_named_table("Debug");
        debug.set_function("DrawLine", [](glm::vec3 start, glm::vec3 end, glm::vec4 color)
        {
            DebugRenderer::DrawLine(start, end, color); 
        });

        debug.set_function("DrawBox", sol::overload(
            [](glm::vec3 min, glm::vec3 max, glm::vec4 color) {
                DebugRenderer::DrawBox(min, max, color);
            },
            [](glm::vec3 center, glm::vec3 size, glm::quat rotation, glm::vec4 color) {
                // Wait, we need to construct the matrix for the OBB version
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), center)
                    * glm::toMat4(rotation)
                    * glm::scale(glm::mat4(1.0f), size);
                DebugRenderer::DrawBox(transform, color);
            }
        ));

        debug.set_function("DrawSphere", [](glm::vec3 center, float radius, glm::vec4 color) {
            DebugRenderer::DrawSphere(center, radius, color);
        });

        debug.set_function("DrawCapsule", [](glm::vec3 center, float radius, float height, glm::quat rotation, glm::vec4 color) {
            DebugRenderer::DrawCapsule(center, radius, height * 0.5f, rotation, color); // Note: Height/2 conversion?
        });
    }

    void ScriptEngine::Shutdown()
    {
        if (m_Data)
        {
            delete m_Data;
            m_Data = nullptr;
        }
    }

    void ScriptEngine::OnEditorStart(Scene* scene, bool recreateScripts)
    {
        m_Data->SceneContext = scene;

        if (recreateScripts)
        {
            auto luaView = scene->Reg().view<LuaScriptComponent>();
            for (auto entity : luaView)
            {
                Entity e = { entity, scene };
                LoadScript(e);
            }
        }
    }

    void ScriptEngine::OnEditorEnd()
    {
        m_Data->SceneContext = nullptr;
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

        if (!lsc.Self.valid())
        {
            LoadScript(entity);
        }

        if (!lsc.Self.valid())
            return;
        
        sol::optional<sol::table> propertiesOption = lsc.Self["Properties"];
        if (propertiesOption)
        {
            sol::table properties = propertiesOption.value();
            for (auto& [key, value] : properties)
            {
                std::string propName = key.as<std::string>();
                sol::table propDef = value.as<sol::table>();

                if (lsc.Self[propName] == sol::nil)
                {
                    lsc.Self[propName] = propDef["Default"];
                }
            }
        }

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

        bool isRuntime = Engine::Get().GetSceneState() == SceneState::Play;

        auto view = m_Data->SceneContext->Reg().view<LuaScriptComponent>();
        for (auto& entityId : view)
        {
            Entity entity { entityId, m_Data->SceneContext };
            auto& lsc = entity.GetComponent<LuaScriptComponent>();

            if (handle == lsc.ScriptHandle)
            {
                LX_CORE_INFO("Reloading script for Entity {0}", (uint64_t)entityId);

                std::unordered_map<std::string, sol::object> backupProps;
                if (lsc.Self.valid())
                {
                    sol::optional<sol::table> propsOpt = lsc.Self["Properties"];
                    if (propsOpt)
                    {
                        sol::table propsDef = propsOpt.value();
                        for (auto& [key, value] : propsDef)
                        {
                            if (key.is<std::string>())
                            {
                                std::string name = key.as<std::string>();
                                sol::object val = lsc.Self[name];
                                if (val.valid())
                                    backupProps[name] = val;
                            }
                        }
                    }

                    if (isRuntime && lsc.Self["OnDestroy"].valid())
                    {
                        sol::protected_function_result result = lsc.Self["OnDestroy"](lsc.Self);
                        if (!result.valid())
                        {
                            sol::error err = result;
                            LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                        }
                    }
                }

                std::string scriptPath = Engine::Get().GetAssetRegistry().Get(lsc.ScriptHandle).FilePath.string();

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

                    sol::optional<sol::table> propertiesOption = lsc.Self["Properties"];
                    if (propertiesOption)
                    {
                        sol::table properties = propertiesOption.value();
                        for (auto& [key, value] : properties)
                        {
                            std::string propName = key.as<std::string>();
                            sol::table propDef = value.as<sol::table>();
                            // Set default
                            lsc.Self[propName] = propDef["Default"];
                        }
                    }

                    for (auto& [name, val] : backupProps)
                    {
                        // Check if the new script still has this property
                        if (propertiesOption && propertiesOption.value()[name].valid())
                        {
                            lsc.Self[name] = val;
                        }
                    }

                    if (isRuntime && lsc.Self["OnCreate"].valid())
                    {
                        sol::protected_function_result createResult = lsc.Self["OnCreate"](lsc.Self);
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
                    LX_CORE_ERROR("Failed to execute script '{0}': {1}", scriptPath, err.what());
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

    void ScriptEngine::LoadScript(Entity entity)
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
        }
        else
        {
            sol::error err = scriptResult;
            LX_CORE_ERROR("Failed to execute script '{0}': {1}", scriptPath.string(), err.what());
        }
    }

    void ScriptEngine::OnActionEvent(const std::string& action, bool pressed)
    {
        auto view = m_Data->SceneContext->Reg().view<LuaScriptComponent>();
        for (auto entity : view)
        {
            auto& lsc = m_Data->SceneContext->Reg().get<LuaScriptComponent>(entity);
            if (lsc.Self.valid())
            {
                sol::protected_function onInput = lsc.Self["OnInput"];
                if (onInput.valid())
                {
                    auto result = onInput(lsc.Self, action, pressed);
                    if (!result.valid())
                    {
                        sol::error err = result;
                        LX_CORE_ERROR("Lua Runtime Error: {0}", err.what());
                    }
                }
            }
        }
    }
}
