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

        m_Data->Lua.new_usertype<glm::vec4>("Color",
            sol::constructors<glm::vec4(float, float, float, float), glm::vec4(float, float, float), glm::vec4(float), glm::vec4()>(),

            "r", &glm::vec4::r,
            "g", &glm::vec4::g,
            "b", &glm::vec4::b,
            "a", &glm::vec4::a,

            "x", &glm::vec4::x,
            "y", &glm::vec4::y,
            "z", &glm::vec4::z,
            "w", &glm::vec4::w,
            
            sol::meta_function::addition, [](const glm::vec4& a, const glm::vec4& b) { return a + b; },
            sol::meta_function::addition, [](const glm::vec4& a, float b) { return a + b; },
            sol::meta_function::subtraction, [](const glm::vec4& a, const glm::vec4& b) { return a - b; },
            sol::meta_function::subtraction, [](const glm::vec4& a, float b) { return a - b; },
            sol::meta_function::multiplication, [](const glm::vec4& a, const glm::vec4& b) { return a * b; },
            sol::meta_function::multiplication, [](const glm::vec4& a, float b) { return a * b; },
            sol::meta_function::division, [](const glm::vec4& a, const glm::vec4& b) { return a / b; },
            sol::meta_function::division, [](const glm::vec4& a, float b) { return a / b; },
            sol::meta_function::unary_minus, [](const glm::vec4& a) { return -a; },
            sol::meta_function::equal_to, [](const glm::vec4& a, const glm::vec4& b) { return a == b; },

            "Lerp", [](const glm::vec4& a, const glm::vec4& b, float t) { return glm::mix(a, b, t); },
            "White", sol::var(glm::vec4(1.0f)),
            "Black", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
            "Red", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            "Green", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
            "Blue", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
        );

        m_Data->Lua.new_usertype<glm::vec3>("Vec3",
            sol::constructors<glm::vec3(float, float, float), glm::vec3(float), glm::vec3()>(),

            "x", &glm::vec3::x,
            "y", &glm::vec3::y,
            "z", &glm::vec3::z,
            
            sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
            sol::meta_function::addition, [](const glm::vec3& a, float b) { return a + b; },
            sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
            sol::meta_function::subtraction, [](const glm::vec3& a, float b) { return a - b; },
            sol::meta_function::multiplication, [](const glm::vec3& a, const glm::vec3& b) { return a * b; },
            sol::meta_function::multiplication, [](const glm::vec3& a, float b) { return a * b; },
            sol::meta_function::division, [](const glm::vec3& a, const glm::vec3& b) { return a / b; },
            sol::meta_function::division, [](const glm::vec3& a, float b) { return a / b; },
            sol::meta_function::unary_minus, [](const glm::vec3& a) { return -a; },
            sol::meta_function::equal_to, [](const glm::vec3& a, const glm::vec3& b) { return a == b; },

            "Length", [](const glm::vec3& v) { return glm::length(v); },
            "Normalize", [](const glm::vec3& v) { return glm::normalize(v); },
            "Distance", [](const glm::vec3& v) { return glm::distance(v, v); },
            "Dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
            "Cross", [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); },
            "Lerp", [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); }
        );

        m_Data->Lua.new_usertype<glm::vec2>("Vec2",
            sol::constructors<glm::vec2(float, float), glm::vec2(float), glm::vec2()>(),

            "x", &glm::vec2::x,
            "y", &glm::vec2::y,
            
            sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
            sol::meta_function::addition, [](const glm::vec2& a, float b) { return a + b; },
            sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
            sol::meta_function::subtraction, [](const glm::vec2& a, float b) { return a - b; },
            sol::meta_function::multiplication, [](const glm::vec2& a, const glm::vec2& b) { return a * b; },
            sol::meta_function::multiplication, [](const glm::vec2& a, float b) { return a * b; },
            sol::meta_function::division, [](const glm::vec2& a, const glm::vec2& b) { return a / b; },
            sol::meta_function::division, [](const glm::vec2& a, float b) { return a / b; },
            sol::meta_function::unary_minus, [](const glm::vec2& a) { return -a; },
            sol::meta_function::equal_to, [](const glm::vec2& a, const glm::vec2& b) { return a == b; },

            "Length", [](const glm::vec2& v) { return glm::length(v); },
            "Normalize", [](const glm::vec2& v) { return glm::normalize(v); },
            "Distance", [](const glm::vec2& v) { return glm::distance(v, v); },
            "Dot", [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); },
            "Lerp", [](const glm::vec2& a, const glm::vec2& b, float t) { return glm::mix(a, b, t); }
        );

        m_Data->Lua.new_usertype<glm::quat>("Quat",
            sol::constructors<glm::quat(), glm::quat(const glm::vec3&), glm::quat(float, float, float, float)>(),

            "w", &glm::quat::w,
            "x", &glm::quat::x,
            "y", &glm::quat::y,
            "z", &glm::quat::z,
            
            sol::meta_function::multiplication, [](const glm::quat& a, const glm::quat& b) { return a * b; },
            sol::meta_function::multiplication, [](const glm::quat& q, const glm::vec3& v) { return q * v; },

            "ToEuler", [](const glm::quat& q) { return glm::eulerAngles(q); },
            "Conjugate", [](const glm::quat& q) { return glm::conjugate(q); },
            "Inverse", [](const glm::quat& q) { return glm::inverse(q); },
            "Normalize", [](const glm::quat& q) { return glm::normalize(q); },
            "Slerp", [](const glm::quat& a, const glm::quat& b, float t) { return glm::slerp(a, b, t); },
            "LookAt", [](const glm::vec3& direction, const glm::vec3& up){ return glm::quatLookAt(direction, up); }
        );

        auto mathTable = m_Data->Lua["Math"].get_or_create<sol::table>();
        mathTable.set_function("Clamp", [](float value, float min, float max) { return glm::clamp(value, min, max); });
        mathTable.set_function("Lerp", [](float a, float b, float t) { return glm::mix(a, b, t); });
        mathTable.set_function("Deg2Rad", [](float deg) { return glm::radians(deg); });
        mathTable.set_function("Rad2Deg", [](float rad) { return glm::degrees(rad); });
        mathTable.set_function("Damp", [](float a, float b, float lambda, float dt)
        {
           return glm::mix(a, b, 1.0f - glm::exp(-lambda * dt)); 
        });
        mathTable.set_function("RandomRange", [](float min, float max)
        {
            // TODO: Simple implementation using std::rand (replace with better engine RNG if we have one)
            float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            return min + r * (max - min);
        });
        mathTable.set_function("RandomInsideUnitSphere", []()
        {
            glm::vec3 p;
            do {
                p = glm::vec3(
                    (float)rand()/RAND_MAX * 2.0f - 1.0f,
                    (float)rand()/RAND_MAX * 2.0f - 1.0f,
                    (float)rand()/RAND_MAX * 2.0f - 1.0f
                );
            } while (glm::length2(p) >= 1.0f);
            return p;
        });
        mathTable.set_function("RandomUnitVector", []()
        {
            glm::vec3 p;
            do {
                p = glm::vec3(
                    (float)rand()/RAND_MAX * 2.0f - 1.0f,
                    (float)rand()/RAND_MAX * 2.0f - 1.0f,
                    (float)rand()/RAND_MAX * 2.0f - 1.0f
                );
            } while (glm::length2(p) >= 1.0f);
            return glm::normalize(p);
        });

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
            "Color", &MeshComponent::Color,
            "MeshHandle", &MeshComponent::Mesh
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
