#pragma once

#include "Lynx/Core.h"
#include "Lynx/Asset/Asset.h"
#include "Lynx/Asset/AssetRef.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/Prefab.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <string>
#include <glm/gtx/matrix_decompose.hpp>

#include "Lynx/Renderer/SceneCamera.h"


namespace Lynx
{
    struct TagComponent
    {
        std::string Tag;

        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct TransformComponent
    {
        glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
        glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

        glm::mat4 WorldMatrix = glm::mat4(1.0f);

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation) : Translation(translation) {}
        
        glm::vec3 GetWorldTranslation() const
        {
            return glm::vec3(WorldMatrix[3]);
        }
        
        glm::quat GetWorldRotation() const
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(WorldMatrix, scale, rotation, translation, skew, perspective);
            return rotation;
        }
        
        glm::vec3 GetWorldScale() const
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(WorldMatrix, scale, rotation, translation, skew, perspective);
            return scale;
        }

        glm::mat4 GetTransform() const
        {
            glm::mat4 rotation = glm::toMat4(Rotation);

            return glm::translate(glm::mat4(1.0f), Translation)
                * rotation
                * glm::scale(glm::mat4(1.0f), Scale);
        }
        
        glm::vec3 GetForward() const
        {
            return glm::rotate(Rotation, glm::vec3(0.0f, 0.0f, 1.0f));
        }
        
        glm::vec3 GetUp() const
        {
            return glm::rotate(Rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        
        glm::vec3 GetRight() const
        {
            return glm::rotate(Rotation, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        
        glm::vec3 GetWorldForward() const
        {
            return glm::normalize(glm::vec3(WorldMatrix[2]));
        }
        
        glm::vec3 GetWorldUp() const
        {
            return glm::normalize(glm::vec3(WorldMatrix[1]));
        }
        
        glm::vec3 GetWorldRight() const
        {
            return glm::normalize(glm::vec3(WorldMatrix[0]));
        }

        glm::vec3 GetRotationDegrees() const
        {
            return glm::degrees(GetRotationEuler());
        }

        void SetRotionDegrees(const glm::vec3& rotDegrees)
        {
            glm::vec3 rotRadians = glm::radians(rotDegrees);
            Rotation = glm::quat(rotRadians);
        }

        glm::vec3 GetRotationEuler() const
        {
            return glm::eulerAngles(Rotation);
        }

        void SetRotationEuler(const glm::vec3& rotation)
        {
            Rotation = glm::quat(rotation);
        }
    };

    struct RelationshipComponent
    {
        entt::entity Parent = entt::null;
        entt::entity FirstChild = entt::null;
        entt::entity NextSibling = entt::null;
        entt::entity PrevSibling = entt::null;

        size_t ChildrenCount = 0;
        
        RelationshipComponent() = default;
        RelationshipComponent(const RelationshipComponent&) = default;
    };
    
    struct DisabledComponent {}; // TODO: We need to check where to actually exclude entities with this component. Should this be visibility only? Or fully disabled?

    struct MeshComponent
    {
        AssetRef<StaticMesh> Mesh;

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
    };

    struct CameraComponent
    {
        SceneCamera Camera;
        bool Primary = true;
        bool FixedAspectRatio = false;

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;

        bool CastShadows = true;

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent&) = default;
    };
    
    struct PrefabComponent
    {
        AssetRef<Prefab> Prefab;
        UUID SubEntityID = UUID::Null();
        
        //std::unordered_set<std::string> Overrides;
        
        PrefabComponent() = default;
        PrefabComponent(const PrefabComponent&) = default;
    };
}
