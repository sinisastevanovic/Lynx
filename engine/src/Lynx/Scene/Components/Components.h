#pragma once

#include "Lynx/Core.h"
#include "Lynx/Asset/Asset.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <string>

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

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation) : Translation(translation) {}

        glm::mat4 GetTransform() const
        {
            glm::mat4 rotation = glm::toMat4(Rotation);

            return glm::translate(glm::mat4(1.0f), Translation)
                * rotation
                * glm::scale(glm::mat4(1.0f), Scale);
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

    struct MeshComponent
    {
        AssetHandle Mesh = AssetHandle::Null();
        
        glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };

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
}
