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

        void RegisterCoreTypes(sol::state& lua)
        {
            lua.new_usertype<Entity>("Entity",
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

            lua.new_usertype<TransformComponent>("Transform",
                "Translation", &TransformComponent::Translation,
                "Scale", &TransformComponent::Scale,
                "Rotation", sol::property(
                    [](TransformComponent& t) { return t.GetRotationDegrees(); },
                    [](TransformComponent& t, const glm::vec3& degrees) { t.SetRotionDegrees(degrees); }
                ),
                "RotationQuat", &TransformComponent::Rotation
            );

            lua.new_usertype<MeshComponent>("MeshComponent",
                "MeshHandle", &MeshComponent::Mesh
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
