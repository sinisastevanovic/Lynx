#include "ScriptWrappers.h"

#include "Lynx/Engine.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Renderer/DebugRenderer.h"
#include "Lynx/Scene/Scene.h"

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Lynx
{
    namespace ScriptWrappers
    {
        void RegisterBasicTypes(sol::state& lua)
        {
            lua.new_usertype<UUID>("UUID",
                sol::constructors<UUID(), UUID(uint64_t)>(),
                "IsValid", &UUID::IsValid,
                sol::meta_function::to_string, &UUID::ToString,
                sol::meta_function::equal_to, &UUID::operator==
            );
            
            lua["AssetHandle"] = lua["UUID"];
        }

        // What a script holds as an entity's transform: the entity, because a write has to go through its scene.
        struct TransformHandle
        {
            Entity Owner;
        };

        void RegisterCoreTypes(sol::state& lua)
        {
            lua.new_usertype<Entity>("Entity",
                sol::constructors<Entity(entt::entity, Scene*)>(),
                "GetID", &Entity::GetUUID,
                "Transform", sol::property([](Entity& entity) { return TransformHandle{ entity }; }),
                "MeshComponent", sol::property(
                    [](Entity& entity) -> MeshComponent*
                    {
                        if (entity.HasComponent<MeshComponent>())
                            return &entity.GetComponent<MeshComponent>();
                        return nullptr;
                    }
                )
            );

            // A script reads a value, changes it and assigns it back: "local p = t.Translation ... t.Translation = p".
            // Changing a part of what it read, as in "t.Translation.x = 1", changes the script's copy only.
            lua.new_usertype<TransformHandle>("Transform",
                "Translation", sol::property(
                    [](const TransformHandle& t) { return t.Owner.GetComponent<TransformComponent>().GetTranslation(); },
                    [](TransformHandle& t, const glm::vec3& translation) { t.Owner.SetTranslation(translation); }
                ),
                "Scale", sol::property(
                    [](const TransformHandle& t) { return t.Owner.GetComponent<TransformComponent>().GetScale(); },
                    [](TransformHandle& t, const glm::vec3& scale) { t.Owner.SetScale(scale); }
                ),
                "Rotation", sol::property(
                    [](const TransformHandle& t) { return t.Owner.GetComponent<TransformComponent>().GetRotationDegrees(); },
                    [](TransformHandle& t, const glm::vec3& degrees) { t.Owner.SetRotation(glm::quat(glm::radians(degrees))); }
                ),
                "RotationQuat", sol::property(
                    [](const TransformHandle& t) { return t.Owner.GetComponent<TransformComponent>().GetRotation(); },
                    [](TransformHandle& t, const glm::quat& rotation) { t.Owner.SetRotation(rotation); }
                )
            );

            lua.new_usertype<MeshComponent>("MeshComponent",
                "MeshHandle", sol::property([](const MeshComponent& mesh) { return mesh.GetMesh().Handle; })
            );
            
            auto world = lua.create_named_table("World");
            world.set_function("Instantiate", [](uint64_t prefabID, sol::optional<Entity> parent) -> Entity
            {
                auto scene = Engine::Get().GetActiveScene();
                if (!scene)
                    return {};
                
                Entity parentEnt = {};
                if (parent)
                    parentEnt = parent.value();
                
                return scene->InstantiatePrefab(AssetHandle(prefabID), parentEnt);
            });
        }

        void RegisterDebug(sol::state& lua)
        {
            auto debug = lua.create_named_table("Debug");
            debug.set_function("DrawLine", [](glm::vec3 start, glm::vec3 end, glm::vec4 color)
            {
                DebugRenderer::DrawLine(start, end, color); 
            });

            debug.set_function("DrawBox", sol::overload(
                [](glm::vec3 min, glm::vec3 max, glm::vec4 color) {
                    DebugRenderer::DrawBox(min, max, color);
                },
                [](glm::vec3 center, glm::vec3 size, glm::quat rotation, glm::vec4 color) {
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
                DebugRenderer::DrawCapsule(center, radius, height * 0.5f, rotation, color);
            });
        }
    }
}
