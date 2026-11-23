#include "CoreComponentRegistration.h"

#include "Components.h"
#include "IDComponent.h"
#include "LuaScriptComponent.h"
#include "NativeScriptComponent.h"
#include "ParticleComponents.h"
#include "PhysicsComponents.h"
#include "UIComponents.h"

#include "Lynx/Engine.h"
#include "Lynx/TypeRegistry.h"
#include "Lynx/Asset/Script.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Scene.h"
#include "Lynx/UI/Core/UIElement.h"

#include <nlohmann/json.hpp>

namespace Lynx
{
    void RegisterCoreComponents(TypeRegistry& registry)
    {
        registry.RegisterCoreInternalOnlyComponent<TransformComponent>("Transform", [](FieldList<TransformComponent>& fields)
        {
            fields.Add("Translation", &TransformComponent::Translation).Label("Position");
            fields.Add("Rotation", &TransformComponent::Rotation);
            fields.Add("Scale", &TransformComponent::Scale);
        });

        // SceneSerializer saves and loads the parent link itself, because it has to resolve the parent by entity ID.
        registry.RegisterCoreInternalOnlyComponent<RelationshipComponent>("Relationship",
            [](entt::registry&, entt::entity, nlohmann::json&) {},
            [](entt::registry&, entt::entity, const nlohmann::json&) {});

        registry.RegisterCoreInternalOnlyComponent<TagComponent>("Tag", [](FieldList<TagComponent>& fields)
        {
            fields.Add("Tag", &TagComponent::Tag);
        });

        registry.RegisterCoreInternalOnlyComponent<PrefabComponent>("Prefab", [](FieldList<PrefabComponent>& fields)
        {
            fields.Add("PrefabHandle", &PrefabComponent::Prefab);
            fields.Add("SubEntityID", &PrefabComponent::SubEntityID);
        });

        registry.RegisterCoreComponent<MeshComponent>("Mesh", [](FieldList<MeshComponent>& fields)
        {
            fields.Add("Mesh", &MeshComponent::Mesh).Label("Static Mesh");
            fields.Add("Materials", &MeshComponent::Materials);
        });

        registry.RegisterCoreComponent<DirectionalLightComponent>("DirectionalLight", [](FieldList<DirectionalLightComponent>& fields)
        {
            fields.Add("Color", &DirectionalLightComponent::Color).AsColor();
            fields.Add("Intensity", &DirectionalLightComponent::Intensity).DragSpeed(0.1f).Range(0.0f, 10000.0f);
            fields.Add("CastShadows", &DirectionalLightComponent::CastShadows);
        });

        registry.RegisterCoreComponent<RigidBodyComponent>("RigidBody", [](FieldList<RigidBodyComponent>& fields)
        {
            fields.Add("Type", &RigidBodyComponent::Type).Names({ "Static", "Dynamic", "Kinematic" });
            fields.Add("Mass", &RigidBodyComponent::Mass);
            fields.Add("Friction", &RigidBodyComponent::Friction);
            fields.Add("Restitution", &RigidBodyComponent::Restitution);
            fields.Add("LinearDamping", &RigidBodyComponent::LinearDamping);
            fields.Add("AngularDamping", &RigidBodyComponent::AngularDamping);
            fields.Add("GravityFactor", &RigidBodyComponent::GravityFactor);
            fields.Add("MotionQuality", &RigidBodyComponent::MotionQuality).Names({ "Discrete", "Continuous" });
            fields.Add("Layer", &RigidBodyComponent::Layer).HideInEditor();
            fields.Add("LockRotationX", &RigidBodyComponent::LockRotationX);
            fields.Add("LockRotationY", &RigidBodyComponent::LockRotationY);
            fields.Add("LockRotationZ", &RigidBodyComponent::LockRotationZ);
        });

        registry.RegisterCoreComponent<BoxColliderComponent>("BoxCollider", [](FieldList<BoxColliderComponent>& fields)
        {
            fields.Add("HalfSize", &BoxColliderComponent::HalfSize).Label("Half Size").Range(0.0f, FLT_MAX);
            fields.Add("Offset", &BoxColliderComponent::Offset);
            fields.Add("IsTrigger", &BoxColliderComponent::IsTrigger);
        });

        registry.RegisterCoreComponent<SphereColliderComponent>("SphereCollider", [](FieldList<SphereColliderComponent>& fields)
        {
            fields.Add("Radius", &SphereColliderComponent::Radius).DragSpeed(0.1f).Range(0.0f, FLT_MAX);
            fields.Add("Offset", &SphereColliderComponent::Offset);
            fields.Add("IsTrigger", &SphereColliderComponent::IsTrigger);
        });

        registry.RegisterCoreComponent<CapsuleColliderComponent>("CapsuleCollider", [](FieldList<CapsuleColliderComponent>& fields)
        {
            fields.Add("Radius", &CapsuleColliderComponent::Radius).DragSpeed(0.1f).Range(0.0f, FLT_MAX);
            fields.Add("HalfHeight", &CapsuleColliderComponent::HalfHeight).Label("Half Height").DragSpeed(0.1f).Range(0.0f, FLT_MAX);
            fields.Add("Offset", &CapsuleColliderComponent::Offset);
            fields.Add("IsTrigger", &CapsuleColliderComponent::IsTrigger);
        });

        registry.RegisterCoreComponent<CharacterControllerComponent>("CharacterController", [](FieldList<CharacterControllerComponent>& fields)
        {
            fields.Add("CapsuleRadius", &CharacterControllerComponent::CapsuleRadius).DragSpeed(0.1f);
            fields.Add("CapsuleHalfHeight", &CharacterControllerComponent::CapsuleHalfHeight).DragSpeed(0.1f);
            fields.Add("MaxSlopeAngle", &CharacterControllerComponent::MaxSlopeAngle).DragSpeed(0.5f).Range(0.0f, 90.0f);
            fields.Add("Mass", &CharacterControllerComponent::Mass).DragSpeed(0.5f).Range(0.0f, 90.0f);
            fields.Add("MaxStrength", &CharacterControllerComponent::MaxStrength).DragSpeed(0.5f).Range(0.0f, FLT_MAX);
            fields.Add("CharacterPadding", &CharacterControllerComponent::CharacterPadding).DragSpeed(0.01f).Range(0.0f, FLT_MAX);
            fields.Add("StepHeight", &CharacterControllerComponent::StepHeight).DragSpeed(0.01f).Range(0.0f, FLT_MAX);
            fields.Add("Gravity", &CharacterControllerComponent::Gravity).DragSpeed(0.1f);
            fields.Add("Layer", &CharacterControllerComponent::Layer).HideInEditor();
        });

        // The components below have logic that a list of fields cannot express, so they are saved and loaded by hand.

        // The camera is configured through setters that depend on its projection type.
        registry.RegisterCoreComponent<CameraComponent>("Camera",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& camComp = reg.get<CameraComponent>(entity);

                auto sceneCamObj = nlohmann::json::object();
                sceneCamObj["ProjectionType"] = camComp.Camera.GetProjectionType();
                sceneCamObj["PerspectiveFOV"] = camComp.Camera.GetPerspectiveVerticalFOV();
                sceneCamObj["PerspectiveNear"] = camComp.Camera.GetPerspectiveNearClip();
                sceneCamObj["PerspectiveFar"] = camComp.Camera.GetPerspectiveFarClip();
                sceneCamObj["OrthographicSize"] = camComp.Camera.GetOrthographicSize();
                sceneCamObj["OrthographicNear"] = camComp.Camera.GetOrthographicNearClip();
                sceneCamObj["OrthographicFar"] = camComp.Camera.GetOrthographicFarClip();
                json["SceneCamera"] = sceneCamObj;
                json["Primary"] = camComp.Primary;
                json["FixedAspectRatio"] = camComp.FixedAspectRatio;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& camComp = reg.get_or_emplace<CameraComponent>(entity);
                camComp.Primary = json["Primary"];
                camComp.FixedAspectRatio = json["FixedAspectRatio"];
                const auto& sceneCamObj = json["SceneCamera"];
                const auto& projectionType = sceneCamObj["ProjectionType"];
                if (projectionType == SceneCamera::ProjectionType::Perspective)
                {
                    camComp.Camera.SetPerspective(
                        sceneCamObj["PerspectiveFOV"],
                        sceneCamObj["PerspectiveNear"],
                        sceneCamObj["PerspectiveFar"]
                    );
                }
                else
                {
                    camComp.Camera.SetOrthographic(
                        sceneCamObj["OrthographicSize"],
                        sceneCamObj["OrthographicNear"],
                        sceneCamObj["OrthographicFar"]
                    );
                }
            });

        // Loading has to bind the script class that the saved name refers to.
        registry.RegisterCoreComponent<NativeScriptComponent>("NativeScript",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& nscComp = reg.get<NativeScriptComponent>(entity);
                json["ScriptName"] = nscComp.ScriptName;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& nscComp = reg.get_or_emplace<NativeScriptComponent>(entity);
                if (json.contains("ScriptName"))
                {
                    std::string scriptName = json["ScriptName"];
                    Engine::Get().GetComponentRegistry().BindScript(scriptName, nscComp);
                }
            });

        // The properties of a script are defined by the script itself and live in its Lua table.
        registry.RegisterCoreComponent<LuaScriptComponent>("LuaScript",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& lscComp = reg.get<LuaScriptComponent>(entity);
                nlohmann::json scriptsArray = nlohmann::json::array();

                for (const auto& instance : lscComp.Scripts)
                {
                    nlohmann::json scriptJson;
                    scriptJson["ScriptAsset"] = instance.ScriptAsset;

                    if (instance.Self.valid())
                    {
                        sol::optional<sol::table> propsOpt = instance.Self["Properties"];
                        if (propsOpt)
                        {
                            auto propsJson = nlohmann::json::object();
                            sol::table propsDef = propsOpt.value();

                            for (auto& [key, value] : propsDef)
                            {
                                std::string name = key.as<std::string>();
                                sol::table def = value.as<sol::table>();
                                std::string type = def["Type"];

                                sol::object runtimeVal = instance.Self[name];
                                if (!runtimeVal.valid())
                                    continue;

                                // TODO: Helper for supported types?
                                if (type == "Float") propsJson[name] = runtimeVal.as<float>();
                                else if (type == "Int") propsJson[name] = runtimeVal.as<int>();
                                else if (type == "String") propsJson[name] = runtimeVal.as<std::string>();
                                else if (type == "Bool") propsJson[name] = runtimeVal.as<bool>();
                                else if (type == "Vec2")
                                {
                                    glm::vec2 vec = runtimeVal.as<glm::vec2>();
                                    propsJson[name] = {vec.x, vec.y};
                                }
                                else if (type == "Vec3")
                                {
                                    glm::vec3 vec = runtimeVal.as<glm::vec3>();
                                    propsJson[name] = {vec.x, vec.y, vec.z};
                                }
                                else if (type == "Color")
                                {
                                    glm::vec4 vec = runtimeVal.as<glm::vec4>();
                                    propsJson[name] = {vec.r, vec.g, vec.b, vec.a};
                                }
                                else if (type == "UIButton" || type == "UIText" || type == "UIImage" || type == "UIElement")
                                {
                                    if (runtimeVal.is<UUID>())
                                        propsJson[name] = runtimeVal.as<UUID>();
                                    else if (runtimeVal.is<UIElement*>())
                                        propsJson[name] = runtimeVal.as<UIElement*>()->GetUUID();
                                }
                            }

                            scriptJson["Properties"] = propsJson;
                        }
                    }
                    scriptsArray.push_back(scriptJson);
                }
                json["Scripts"] = scriptsArray;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& lscComp = reg.get_or_emplace<LuaScriptComponent>(entity);

                if (json.contains("Scripts"))
                {
                    Scene* scene = reg.ctx().get<Scene*>();
                    Entity e{ entity, scene };

                    for (const auto& scriptJson : json["Scripts"])
                    {
                        ScriptInstance instance;
                        instance.ScriptAsset = scriptJson["ScriptAsset"].get<AssetRef<Script>>();

                        if (scene && instance.ScriptAsset)
                        {
                            if (Engine::Get().GetScriptEngine()->InstantiateScript(e, instance))
                            {
                                if (scriptJson.contains("Properties"))
                                {
                                    const auto& propsJson = scriptJson["Properties"];

                                    if (!instance.Self.valid())
                                        continue;

                                    std::optional<sol::table> propsOpt = instance.Self["Properties"];
                                    if (propsOpt)
                                    {
                                        sol::table propsDef = propsOpt.value();
                                        for (auto& [key, value] : propsDef)
                                        {
                                            std::string name = key.as<std::string>();
                                            if (!propsJson.contains(name))
                                                continue;
                                            sol::table def = value.as<sol::table>();
                                            std::string type = def["Type"];
                                            if (type == "Float") instance.Self[name] = propsJson[name].get<float>();
                                            else if (type == "Int") instance.Self[name] = propsJson[name].get<int>();
                                            else if (type == "String") instance.Self[name] = propsJson[name].get<std::string>();
                                            else if (type == "Bool") instance.Self[name] = propsJson[name].get<bool>();
                                            else if (type == "Vec2")
                                            {
                                                const auto& jsonVal = propsJson[name];
                                                instance.Self[name] = glm::vec2(jsonVal[0], jsonVal[1]);
                                            }
                                            else if (type == "Vec3")
                                            {
                                                const auto& jsonVal = propsJson[name];
                                                instance.Self[name] = glm::vec3(jsonVal[0], jsonVal[1], jsonVal[2]);
                                            }
                                            else if (type == "Color")
                                            {
                                                const auto& jsonVal = propsJson[name];
                                                instance.Self[name] = glm::vec4(jsonVal[0], jsonVal[1], jsonVal[2], jsonVal[3]);
                                            }
                                            else if (type == "UIButton" || type == "UIText" || type == "UIImage" || type == "UIElement")
                                            {
                                                instance.Self[name] = propsJson[name].get<UUID>();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        lscComp.Scripts.push_back(instance);
                    }
                }
            });

        // Changing the particle count has to resize the particle pool.
        registry.RegisterCoreComponent<ParticleEmitterComponent>("ParticleEmitter",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& comp = reg.get<ParticleEmitterComponent>(entity);
                json["Material"] = static_cast<uint64_t>(comp.Material.Handle);
                json["MaxParticles"] = comp.MaxParticles;
                json["EmissionRate"] = comp.EmissionRate;
                json["IsLooping"] = comp.IsLooping;
                json["DepthSorting"] = comp.DepthSorting;

                auto props = nlohmann::json::object();
                props["Position"] = { comp.Properties.Position.x, comp.Properties.Position.y, comp.Properties.Position.z };
                props["Velocity"] = { comp.Properties.Velocity.x, comp.Properties.Velocity.y, comp.Properties.Velocity.z };
                props["VelocityVariation"] = { comp.Properties.VelocityVariation.x, comp.Properties.VelocityVariation.y, comp.Properties.VelocityVariation.z };
                props["ColorBegin"] = { comp.Properties.ColorBegin.r, comp.Properties.ColorBegin.g, comp.Properties.ColorBegin.b, comp.Properties.ColorBegin.a };
                props["ColorEnd"] = { comp.Properties.ColorEnd.r, comp.Properties.ColorEnd.g, comp.Properties.ColorEnd.b, comp.Properties.ColorEnd.a };
                props["SizeBegin"] = comp.Properties.SizeBegin;
                props["SizeEnd"] = comp.Properties.SizeEnd;
                props["SizeVariation"] = comp.Properties.SizeVariation;
                props["LifeTime"] = comp.Properties.LifeTime;

                json["Properties"] = props;
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& comp = reg.get_or_emplace<ParticleEmitterComponent>(entity);
                if (json.contains("Material")) comp.Material = AssetHandle(json["Material"].get<uint64_t>());
                if (json.contains("MaxParticles"))
                {
                    comp.SetMaxParticles(json["MaxParticles"]);
                }
                if (json.contains("EmissionRate")) comp.EmissionRate = json["EmissionRate"];
                if (json.contains("IsLooping")) comp.IsLooping = json["IsLooping"];
                if (json.contains("DepthSorting")) comp.DepthSorting = json["DepthSorting"];

                if (json.contains("Properties"))
                {
                    auto props = json["Properties"];
                    // Helpers for array reading
                    auto getVec3 = [](const nlohmann::json& j) { return glm::vec3(j[0], j[1], j[2]); };
                    auto getVec4 = [](const nlohmann::json& j) { return glm::vec4(j[0], j[1], j[2], j[3]); };

                    comp.Properties.Position = getVec3(props["Position"]);
                    comp.Properties.Velocity = getVec3(props["Velocity"]);
                    comp.Properties.VelocityVariation = getVec3(props["VelocityVariation"]);
                    comp.Properties.ColorBegin = getVec4(props["ColorBegin"]);
                    comp.Properties.ColorEnd = getVec4(props["ColorEnd"]);
                    comp.Properties.SizeBegin = props["SizeBegin"];
                    comp.Properties.SizeEnd = props["SizeEnd"];
                    comp.Properties.SizeVariation = props["SizeVariation"];
                    comp.Properties.LifeTime = props["LifeTime"];
                }
            });

        // The canvas saves and loads its own tree of UI elements.
        registry.RegisterCoreComponent<UICanvasComponent>("UICanvas",
            [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
            {
                auto& comp = reg.get<UICanvasComponent>(entity);
                if (comp.Canvas)
                {
                    nlohmann::json canvasJson;
                    // Recursively save the entire UI Tree
                    comp.Canvas->Serialize(canvasJson);
                    json["Canvas"] = canvasJson;
                    json["SortingOrder"] = comp.SortingOrder;
                }
            },
            [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
            {
                auto& comp = reg.get_or_emplace<UICanvasComponent>(entity);
                if (json.contains("Canvas"))
                {
                    comp.SortingOrder = json.value("SortingOrder", 0);
                    // Create a new Canvas (which is also the root UIElement)
                    comp.Canvas = std::make_shared<UICanvas>();
                    // Recursively load the tree
                    comp.Canvas->Deserialize(json["Canvas"]);
                    comp.Canvas->OnPostLoad();
                }
            });
    }
}
