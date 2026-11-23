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

#include <map>
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

    // The matrix that scales, then rotates, then moves. It is what multiplying a translation, a rotation
    // and a scale matrix gives, written out: the three axes of the rotation, each stretched by its scale,
    // and the position. That is a fraction of the arithmetic of the two matrix products.
    inline glm::mat4 ComposeTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
    {
        const glm::mat3 axes = glm::mat3_cast(rotation);
        return glm::mat4(
            glm::vec4(axes[0] * scale.x, 0.0f),
            glm::vec4(axes[1] * scale.y, 0.0f),
            glm::vec4(axes[2] * scale.z, 0.0f),
            glm::vec4(translation, 1.0f));
    }

    struct TransformComponent
    {
        glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
        glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
        
        glm::mat4 WorldMatrix = glm::mat4(1.0f);

        // What the scene needs to compute the world matrix only when it has to: the values it was last
        // computed from, and a mark for when it has to be computed whatever they are, as for a new entity
        // or one that got another parent. Written by the scene, not saved.
        glm::vec3 ComputedTranslation{ 0.0f };
        glm::quat ComputedRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 ComputedScale{ 1.0f };
        bool NeedsWorldMatrix = true;
        
        // For physics interpolation
        glm::vec3 PreviousTranslation{0.0f};
        glm::quat PreviousRotation{1.0f, 0.0f, 0.0f, 0.0f};

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
            return ComposeTransform(Translation, Rotation, Scale);
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
        
        glm::vec3 GetPhysicsInterpolatedTranslation(float alpha) const
        {
            return glm::mix(PreviousTranslation, Translation, alpha);
        }
        
        glm::quat GetPhysicsInterpolatedRotation(float alpha) const
        {
            return glm::slerp(PreviousRotation, Rotation, alpha);
        }
        
        glm::mat4 GetPhysicsInterpolatedTransform(float alpha) const
        {
            return ComposeTransform(GetPhysicsInterpolatedTranslation(alpha), GetPhysicsInterpolatedRotation(alpha), Scale);
        }
    };
    
    struct WorldTransformComponent
    {
        glm::vec3 WorldPosition = {0.0f, 0.0f, 0.0f};
        glm::quat WorldRotation = {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 WorldScale = {1.0f, 1.0f, 1.0f};
        
        glm::mat4 WorldMatrix{1.0f};
    };
    
    struct PhysicsStateComponent
    {
        glm::vec3 PrevPosition;
        glm::quat PrevRotation;
        
        glm::vec3 CurrPosition;
        glm::quat CurrRotation;
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

    // Component data whose type was not registered when the entity was loaded, for example because
    // the game module was missing. It is kept as JSON text per component name and written back on
    // save, so loading and saving a scene never deletes data the engine does not understand.
    struct UnregisteredDataComponent
    {
        std::map<std::string, std::string> SerializedByName;
    };

    struct MeshComponent
    {
        AssetRef<StaticMesh> Mesh;

        // Materials that take the place of the mesh's own: one per submesh, in the mesh's order.
        // An empty reference keeps the material the mesh came with, and so does a submesh past the end
        // of the list. This is how many entities share one mesh and still look different.
        std::vector<AssetRef<Material>> Materials;

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
