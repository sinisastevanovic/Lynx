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

    template<typename T> class FieldList;
    class Scene;

    // Where an entity is, relative to its parent: position, rotation and scale.
    //
    // They are read here and written through the scene: Scene::SetTranslation and its siblings, or the
    // same calls on Entity. A write has to reach the scene, because the scene then works out where the
    // entity and its children are in the world and tells the renderer. In return nothing ever looks at
    // an entity that was not written to: a scene of 100,000 entities of which 50 move costs 50.
    class TransformComponent
    {
    public:
        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation) : m_Translation(translation) {}

        const glm::vec3& GetTranslation() const { return m_Translation; }
        const glm::quat& GetRotation() const { return m_Rotation; }
        const glm::vec3& GetScale() const { return m_Scale; }

        // Where the entity is in the world, through every parent, as of the scene's last update of
        // transforms. A write to this entity or to a parent shows here after the next one.
        const glm::mat4& GetWorldMatrix() const { return m_WorldMatrix; }

        glm::vec3 GetWorldTranslation() const
        {
            return glm::vec3(m_WorldMatrix[3]);
        }

        glm::quat GetWorldRotation() const
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(m_WorldMatrix, scale, rotation, translation, skew, perspective);
            return rotation;
        }

        glm::vec3 GetWorldScale() const
        {
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(m_WorldMatrix, scale, rotation, translation, skew, perspective);
            return scale;
        }

        // Relative to the parent.
        glm::mat4 GetTransform() const
        {
            return ComposeTransform(m_Translation, m_Rotation, m_Scale);
        }

        glm::vec3 GetForward() const
        {
            return glm::rotate(m_Rotation, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        glm::vec3 GetUp() const
        {
            return glm::rotate(m_Rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        glm::vec3 GetRight() const
        {
            return glm::rotate(m_Rotation, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        glm::vec3 GetWorldForward() const
        {
            return glm::normalize(glm::vec3(m_WorldMatrix[2]));
        }

        glm::vec3 GetWorldUp() const
        {
            return glm::normalize(glm::vec3(m_WorldMatrix[1]));
        }

        glm::vec3 GetWorldRight() const
        {
            return glm::normalize(glm::vec3(m_WorldMatrix[0]));
        }

        glm::vec3 GetRotationEuler() const
        {
            return glm::eulerAngles(m_Rotation);
        }

        glm::vec3 GetRotationDegrees() const
        {
            return glm::degrees(GetRotationEuler());
        }

        glm::vec3 GetPhysicsInterpolatedTranslation(float alpha) const
        {
            return glm::mix(PreviousTranslation, m_Translation, alpha);
        }

        glm::quat GetPhysicsInterpolatedRotation(float alpha) const
        {
            return glm::slerp(PreviousRotation, m_Rotation, alpha);
        }

        glm::mat4 GetPhysicsInterpolatedTransform(float alpha) const
        {
            return ComposeTransform(GetPhysicsInterpolatedTranslation(alpha), GetPhysicsInterpolatedRotation(alpha), m_Scale);
        }

        // The fields that are saved, shown in the inspector and reachable from scripts.
        static void DescribeFields(FieldList<TransformComponent>& fields);

        // Where the entity was at the physics step before the last one, written by the physics system.
        // A frame between two steps is drawn between the two poses.
        glm::vec3 PreviousTranslation{ 0.0f };
        glm::quat PreviousRotation{ 1.0f, 0.0f, 0.0f, 0.0f };

    private:
        friend class Scene;

        glm::vec3 m_Translation = { 0.0f, 0.0f, 0.0f };
        glm::quat m_Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 m_Scale = { 1.0f, 1.0f, 1.0f };
        glm::mat4 m_WorldMatrix = glm::mat4(1.0f);

        // Set while the entity waits in the scene's list of transforms that were written to.
        bool m_IsChanged = false;

        // Whether the entity has a parent or children, kept in step by the scene. Most entities have
        // neither, and for those the scene need not look at the hierarchy at all.
        bool m_HasRelatives = false;
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

    // The mesh an entity is drawn with. Read here and written through the scene (Scene::SetMesh,
    // Scene::SetMaterial), so that the renderer hears of a change instead of looking for one.
    class MeshComponent
    {
    public:
        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;

        const AssetRef<StaticMesh>& GetMesh() const { return m_Mesh; }

        // Materials that take the place of the mesh's own: one per submesh, in the mesh's order.
        // An empty reference keeps the material the mesh came with, and so does a submesh past the end
        // of the list. This is how many entities share one mesh and still look different.
        const std::vector<AssetRef<Material>>& GetMaterials() const { return m_Materials; }

        // The fields that are saved and reachable from scripts.
        static void DescribeFields(FieldList<MeshComponent>& fields);

    private:
        friend class Scene;

        AssetRef<StaticMesh> m_Mesh;
        std::vector<AssetRef<Material>> m_Materials;
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
