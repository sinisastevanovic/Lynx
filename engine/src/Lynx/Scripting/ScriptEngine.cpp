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
        
        auto world = m_Data->Lua.create_named_table("World");
        world.set_function("Instantiate", [this](uint64_t prefabID, sol::optional<Entity> parent) -> Entity
        {
            if (!this->m_Data->SceneContext)
                return {};
            
            Entity parentEnt = {};
            if (parent)
                parentEnt = parent.value();
            
            return this->m_Data->SceneContext->InstantiatePrefab(AssetHandle(prefabID), parentEnt);
        });
    }

    void ScriptEngine::Shutdown()
    {
        delete m_Data;
        m_Data = nullptr;
    }

    void ScriptEngine::OnEditorStart(Scene* scene)
    {
        LX_CORE_INFO("Script Editor start!");
        m_Data->SceneContext = scene;
    }
    
    void ScriptEngine::OnEditorEnd()
    {
        LX_CORE_INFO("Script Editor stop!");
        m_Data->SceneContext = nullptr;
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        LX_CORE_INFO("Script Runtime start!");
        m_Data->SceneContext = scene;
        auto view = scene->Reg().view<LuaScriptComponent>();
        for (auto entityID : view)
        {
            Entity entity{ entityID, scene };
            auto& comp = view.get<LuaScriptComponent>(entityID);
            for (auto& instance : comp.Scripts)
            {
                if (!instance.Self.valid())
                {
                    InstantiateScript(entity, instance);
                    InitializeProperties(instance);
                }
                CallMethod(instance, "OnCreate");
            }
        }
    }

    void ScriptEngine::OnRuntimeStop()
    {
        LX_CORE_INFO("Script Runtime stopped!");
        if (m_Data->SceneContext)
        {
            auto view = m_Data->SceneContext->Reg().view<LuaScriptComponent>();
            for (auto entityID : view)
            {
                auto& comp = view.get<LuaScriptComponent>(entityID);
                for (auto& instance : comp.Scripts)
                {
                    if (instance.Self.valid())
                    {
                        CallMethod(instance, "OnDestroy");
                    }
                }
            }
        }
        m_Data->SceneContext = nullptr;
    }

    void ScriptEngine::OnScriptComponentAdded(Entity entity)
    {
        LX_CORE_INFO("Script Component Added!");
        auto& comp = entity.GetComponent<LuaScriptComponent>();
        for (auto& instance : comp.Scripts)
        {
            if (!instance.Self.valid())
            {
                if (InstantiateScript(entity, instance))
                {
                    InitializeProperties(instance);
                    if (Engine::Get().GetSceneState() == SceneState::Play)
                    {
                        CallMethod(instance, "OnCreate");
                    }
                }
            }
        }
    }

    void ScriptEngine::OnScriptComponentDestroyed(Entity entity)
    {
        LX_CORE_INFO("Script Component Removed!");
        auto& comp = entity.GetComponent<LuaScriptComponent>();
        for (auto& instance : comp.Scripts)
        {
            if (Engine::Get().GetSceneState() == SceneState::Play)
            {
                CallMethod(instance, "OnDestroy");
            }
            instance.Self = sol::nil;
        }
    }

    bool ScriptEngine::InstantiateScript(Entity entity, ScriptInstance& instance)
    {
        if (!instance.ScriptAsset)
            return false;
        
        auto scriptAsset = instance.ScriptAsset.Get();
        if (!scriptAsset)
            return false;
        
        std::filesystem::path scriptPath = scriptAsset->GetFilePath();
        if (scriptPath.empty())
            return false;
        
        sol::load_result result = m_Data->Lua.load_file(scriptPath.string());
        if (!result.valid())
        {
            sol::error err = result;
            LX_CORE_ERROR("Failed to load script '{0}': {1}", scriptPath.string(), err.what());
            return false;
        }
        
        sol::protected_function_result scriptResult = result();
        if (scriptResult.valid())
        {
            instance.Self = scriptResult.get<sol::table>();
            instance.Self["CurrentEntity"] = entity;
            return true;
        }
        else
        {
            sol::error err = scriptResult;
            LX_CORE_ERROR("Failed to instantiate script '{0}': {1}", scriptPath.string(), err.what());
            return false;
        }
    }

    void ScriptEngine::InitializeProperties(ScriptInstance& instance)
    {
        if (!instance.Self.valid())
            return;
        
        sol::optional<sol::table> propertiesOption = instance.Self["Properties"];
        if (propertiesOption)
        {
            sol::table properties = propertiesOption.value();
            for (auto& [key, value] : properties)
            {
                std::string propName = key.as<std::string>();
                sol::table propDef = value.as<sol::table>();
                
                if (instance.Self[propName] == sol::nil)
                    instance.Self[propName] = propDef["Default"];
            }
        }
    }
    
    void ScriptEngine::ReloadScript(AssetHandle handle)
    {
        if (!m_Data->SceneContext)
            return;
        
        bool isRuntime = Engine::Get().GetSceneState() == SceneState::Play;
        auto view = m_Data->SceneContext->Reg().view<LuaScriptComponent>();
        
        for (auto& entityID : view)
        {
            Entity entity{ entityID, m_Data->SceneContext };
            auto& comp = view.get<LuaScriptComponent>(entityID);
            for (auto& instance : comp.Scripts)
            {
                if (handle == instance.ScriptAsset.Handle)
                {
                    LX_CORE_INFO("Reloading script for Entity {0}", (uint64_t)entityID);
                
                    std::unordered_map<std::string, sol::object> backupProps;
                    if (instance.Self.valid())
                    {
                        sol::optional<sol::table> propsOpt = instance.Self["Properties"];
                        if (propsOpt)
                        {
                            for (auto& [key, value] : propsOpt.value())
                            {
                                std::string name = key.as<std::string>();
                                sol::object val = instance.Self[name];
                                if (val.valid()) backupProps[name] = val;
                            }
                        }

                        if (isRuntime) 
                            CallMethod(instance, "OnDestroy");
                    }
                
                    if (InstantiateScript(entity, instance))
                    {
                        InitializeProperties(instance);
                        for (auto& [name, val] : backupProps)
                        {
                            sol::optional<sol::table> newProps = instance.Self["Properties"];
                            if (newProps && newProps.value()[name].valid())
                            {
                                instance.Self[name] = val;
                            }
                        }
                        if (isRuntime) 
                            CallMethod(instance, "OnCreate");
                    }
                }
            }
        }
    }
    
    void ScriptEngine::OnUpdateEntity(Entity entity, float deltaTime)
    {
        auto& comp = entity.GetComponent<LuaScriptComponent>();
        for (auto& instance : comp.Scripts)
        {
            CallMethod(instance, "OnUpdate", deltaTime);
        }
    }
    
    void ScriptEngine::OnActionEvent(const std::string& action, bool pressed)
    {
        if (!m_Data->SceneContext)
            return;
        
        auto view = m_Data->SceneContext->Reg().view<LuaScriptComponent>();
        for (auto entityID : view)
        {
            Entity entity{ entityID, m_Data->SceneContext };
            auto& comp = view.get<LuaScriptComponent>(entityID);
            for (auto& instance : comp.Scripts)
            {
                CallMethod(instance, "OnInput", action, pressed);
            }
        }
    }
    
    template <typename ... Args>
    void ScriptEngine::CallMethod(ScriptInstance& instance, const std::string& methodName, Args&&... args)
    {
        if (!instance.Self.valid())
            return;
        
        sol::protected_function func = instance.Self[methodName];
        if (func.valid())
        {
            auto result = func(instance.Self, std::forward<Args>(args)...);
            if (!result.valid())
            {
                sol::error err = result;
                LX_CORE_ERROR("Lua Runtime Error ({0}): {1}", methodName, err.what());
            }
        }
    }
}
