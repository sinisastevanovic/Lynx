#include "Scene.h"
#include "Components/Components.h"
#include "Components/PhysicsComponents.h"
#include "Entity.h"
#include "Lynx/Engine.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "Components/LuaScriptComponent.h"
#include "Components/NativeScriptComponent.h"
#include "Lynx/Scripting/ScriptEngine.h"
#include <glm/gtx/matrix_decompose.hpp>

#include "SceneSerializer.h"
#include "Components/UIComponents.h"

namespace Lynx
{
    static void SetTransformFromMatrix(TransformComponent& transform, const glm::mat4& matrix)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;

        glm::decompose(matrix, scale, rotation, translation, skew, perspective);
        transform.Translation = translation;
        transform.Rotation = rotation;
        transform.Scale = scale;
    }
    
    static void OnRigidBodyComponentDestroyed(entt::registry& registry, entt::entity entity)
    {
        auto& rb = registry.get<RigidBodyComponent>(entity);
        if (rb.RuntimeBodyCreated)
        {
            auto& physicsSystem = Engine::Get().GetPhysicsSystem();
            auto& bodyInterface = physicsSystem.GetBodyInterface();

            bodyInterface.RemoveBody(rb.BodyId);
            bodyInterface.DestroyBody(rb.BodyId);
        }
    }

    static void OnNativeScriptComponentDestroyed(entt::registry& registry, entt::entity entity)
    {
        auto& nsc = registry.get<NativeScriptComponent>(entity);
        if (nsc.Instance)
        {
            nsc.Instance->OnDestroy();
            nsc.DestroyScript(&nsc);
        }
    }

    static void OnLuaScriptComponentDestroyed(entt::registry& registry, entt::entity entity)
    {
        if (Engine::Get().GetSceneState() == SceneState::Play)
        {
            Scene* scene = Engine::Get().GetActiveScene().get();
            if (scene)
            {
                Entity e{entity, scene};
                Engine::Get().GetScriptEngine()->OnDestroyEntity(e);
            }
        }
    }
    
    Scene::Scene(const std::string& filePath)
        : Asset(filePath)
    {
        m_Registry.ctx().emplace<Scene*>(this);
        
        m_Registry.on_destroy<RigidBodyComponent>().connect<&OnRigidBodyComponentDestroyed>();
        m_Registry.on_destroy<NativeScriptComponent>().connect<&OnNativeScriptComponentDestroyed>();
        m_Registry.on_destroy<LuaScriptComponent>().connect<&OnLuaScriptComponentDestroyed>();
    }

    Scene::~Scene()
    {
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>();
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::DestroyEntity(entt::entity entity, bool excludeChildren)
    {
        if (!m_Registry.valid(entity))
            return;

        auto& rel = m_Registry.get<RelationshipComponent>(entity);
        entt::entity child = rel.FirstChild;
        while (child != entt::null)
        {
            entt::entity nextChild = m_Registry.get<RelationshipComponent>(child).NextSibling;
            if (excludeChildren)
            {
                DetachEntityKeepWorld(child);
            }
            else
            {
                DestroyEntity(child, false);
            }
            child = nextChild;
        }
        
        DetachEntity(entity);
        m_Registry.destroy(entity);
    }

    void Scene::OnRuntimeStart()
    {
        UpdateGlobalTransforms();
        
        Engine::Get().GetScriptEngine()->OnRuntimeStart(this);
    
        auto& physicsSystem = Engine::Get().GetPhysicsSystem();
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

        {
            auto view = m_Registry.view<TransformComponent, RigidBodyComponent, RelationshipComponent>();
            std::vector<entt::entity> entitiesToDetach;

            for (auto entity : view)
            {
                auto [transform, rb, rel] = view.get<TransformComponent, RigidBodyComponent, RelationshipComponent>(entity);
                if (rel.Parent != entt::null)
                {
                    entitiesToDetach.push_back(entity);
                }
            }

            for (auto entity : entitiesToDetach)
            {
                DetachEntityKeepWorld(entity);
                LX_CORE_WARN("Entity with RigidBody was detached from hierarchy at runtime start.");
            }
        }

        auto view = m_Registry.view<TransformComponent, RigidBodyComponent>();
        for (auto entity : view)
        {
            auto [transform, rb] = view.get<TransformComponent, RigidBodyComponent>(entity);
            if (!rb.RuntimeBodyCreated)
            {
                JPH::ShapeRefC shape;

                if (m_Registry.all_of<BoxColliderComponent>(entity))
                {
                    auto& collider = m_Registry.get<BoxColliderComponent>(entity);
                    shape = new JPH::BoxShape(JPH::Vec3(collider.HalfSize.x, collider.HalfSize.y, collider.HalfSize.z));
                }
                else if (m_Registry.all_of<CapsuleColliderComponent>(entity))
                {
                    auto& collider = m_Registry.get<CapsuleColliderComponent>(entity);
                    float halfHeight = collider.HalfHeight - collider.Radius;
                    if (halfHeight <= 0.0f)
                        halfHeight = 0.01f;

                    shape = new JPH::CapsuleShape(halfHeight, collider.Radius);
                }
                else if (m_Registry.all_of<SphereColliderComponent>(entity))
                {
                    auto& collider = m_Registry.get<SphereColliderComponent>(entity);
                    shape = new JPH::SphereShape(collider.Radius);
                }
                else
                {
                    LX_CORE_WARN("Entity '{0}' has Rigidbody but no Collider! Defaulting to small box.", (uint32_t)entity);
                    shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
                }
                
                // TODO: Add kinematic!
                JPH::ObjectLayer layer = (rb.Type == RigidBodyType::Static) ? Layers::STATIC : Layers::MOVABLE;
                JPH::EMotionType motion = (rb.Type == RigidBodyType::Static) ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;

                JPH::Vec3 pos(transform.Translation.x, transform.Translation.y, transform.Translation.z);
                JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

                JPH::BodyCreationSettings bodySettings(shape, pos, rot, motion, layer);

                JPH::EAllowedDOFs allowedDOFs = JPH::EAllowedDOFs::All;
                if (rb.LockRotationX)
                    allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationX;
                if (rb.LockRotationY)
                    allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationY;
                if (rb.LockRotationZ)
                    allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationZ;
                bodySettings.mAllowedDOFs = allowedDOFs;

                JPH::Body* body = bodyInterface.CreateBody(bodySettings);

                bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);

                rb.BodyId = body->GetID();
                rb.RuntimeBodyCreated = true;
            }
        }

        auto nscView = m_Registry.view<NativeScriptComponent>();
        for (auto entity : nscView)
        {
            auto& nsc = m_Registry.get<NativeScriptComponent>(entity);
            if (!nsc.Instance && nsc.InstantiateScript)
            {
                nsc.Instance = nsc.InstantiateScript();
                nsc.Instance->m_Entity = Entity{ entity, this };
                nsc.Instance->OnCreate();
            }
        }

        auto luaView = m_Registry.view<LuaScriptComponent>();
        for (auto entity : luaView)
        {
            Entity e = { entity, this };
            Engine::Get().GetScriptEngine()->OnCreateEntity(e);
        }
    }

    void Scene::OnRuntimeStop()
    {
        auto luaView = m_Registry.view<LuaScriptComponent>();
        for (auto entity : luaView)
        {
            Entity e = { entity, this };
            Engine::Get().GetScriptEngine()->OnDestroyEntity(e);
        }

        Engine::Get().GetScriptEngine()->OnRuntimeStop();

        auto nscView = m_Registry.view<NativeScriptComponent>();
        for (auto entity : nscView)
        {
            auto& nsc = m_Registry.get<NativeScriptComponent>(entity);
            if (nsc.Instance)
            {
                nsc.Instance->OnDestroy();
                nsc.DestroyScript(&nsc);
            }
        }
        
        auto& physicsSystem = Engine::Get().GetPhysicsSystem();
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

        auto view = m_Registry.view<RigidBodyComponent>();
        for (auto entity : view)
        {
            auto& rb = view.get<RigidBodyComponent>(entity);
            if (rb.RuntimeBodyCreated)
            {
                bodyInterface.RemoveBody(rb.BodyId);
                bodyInterface.DestroyBody(rb.BodyId);
                rb.RuntimeBodyCreated = false;
            }
        }
    }

    void Scene::OnUpdateRuntime(float deltaTime)
    {
        auto luaView = m_Registry.view<LuaScriptComponent>();
        for (auto entity : luaView)
        {
            Entity e = { entity, this };
            Engine::Get().GetScriptEngine()->OnUpdateEntity(e, deltaTime);
        }
        
        auto nscView = m_Registry.view<NativeScriptComponent>();
        for (auto entity : nscView)
        {
            auto& nsc = m_Registry.get<NativeScriptComponent>(entity);
            if (nsc.Instance)
            {
                nsc.Instance->OnUpdate(deltaTime);
            }
        }
        
        auto& physicsSystem = Engine::Get().GetPhysicsSystem();
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

        // Sync physics -> transforms
        auto view = m_Registry.view<TransformComponent, RigidBodyComponent, RelationshipComponent>();
        for (auto entity : view)
        {
            auto [transform, rb, rel] = view.get<TransformComponent, RigidBodyComponent, RelationshipComponent>(entity);
            
            if (!rb.RuntimeBodyCreated)
            {
                JPH::ShapeRefC shape;

                if (m_Registry.all_of<BoxColliderComponent>(entity))
                {
                    auto& collider = m_Registry.get<BoxColliderComponent>(entity);
                    shape = new JPH::BoxShape(JPH::Vec3(collider.HalfSize.x, collider.HalfSize.y, collider.HalfSize.z));
                }
                else if (m_Registry.all_of<CapsuleColliderComponent>(entity))
                {
                    auto& collider = m_Registry.get<CapsuleColliderComponent>(entity);
                    float halfHeight = collider.HalfHeight - collider.Radius;
                    if (halfHeight <= 0.0f)
                        halfHeight = 0.01f;

                    shape = new JPH::CapsuleShape(halfHeight, collider.Radius);
                }
                else if (m_Registry.all_of<SphereColliderComponent>(entity))
                {
                    auto& collider = m_Registry.get<SphereColliderComponent>(entity);
                    shape = new JPH::SphereShape(collider.Radius);
                }
                else
                {
                    LX_CORE_WARN("Entity '{0}' has Rigidbody but no Collider! Defaulting to small box.", (uint32_t)entity);
                    shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
                }
                
                // TODO: Add kinematic!
                JPH::ObjectLayer layer = (rb.Type == RigidBodyType::Static) ? Layers::STATIC : Layers::MOVABLE;
                JPH::EMotionType motion = (rb.Type == RigidBodyType::Static) ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;

                JPH::Vec3 pos(transform.Translation.x, transform.Translation.y, transform.Translation.z);
                JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

                JPH::BodyCreationSettings bodySettings(shape, pos, rot, motion, layer);

                JPH::EAllowedDOFs allowedDOFs = JPH::EAllowedDOFs::All;
                if (rb.LockRotationX)
                    allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationX;
                if (rb.LockRotationY)
                    allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationY;
                if (rb.LockRotationZ)
                    allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationZ;
                bodySettings.mAllowedDOFs = allowedDOFs;

                JPH::Body* body = bodyInterface.CreateBody(bodySettings);

                bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);

                rb.BodyId = body->GetID();
                rb.RuntimeBodyCreated = true;
            }
            
            if (rb.RuntimeBodyCreated && rb.Type == RigidBodyType::Dynamic)
            {
                if (bodyInterface.IsAdded(rb.BodyId) && bodyInterface.IsActive(rb.BodyId))
                {
                    JPH::Vec3 pos = bodyInterface.GetPosition(rb.BodyId);
                    JPH::Quat rot = bodyInterface.GetRotation(rb.BodyId);

                    transform.Translation = { pos.GetX(), pos.GetY(), pos.GetZ() };
                    transform.Rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
                }
            }
        }
    }

    void Scene::OnUpdateEditor(float deltaTime)
    {
    }

    void Scene::UpdateUILayout(uint32_t viewportWidth, uint32_t viewportHeight, const glm::mat4& viewProj, const glm::vec3& cameraPos)
    {
        // TODO: Do we really need to do this every frame? At least for screen space UI, we could cache it, right? 
        auto view = m_Registry.view<CanvasComponent, RectTransformComponent, RelationshipComponent>();
        for (auto entity : view)
        {
            auto [canvas, rect, relation] = view.get<CanvasComponent, RectTransformComponent, RelationshipComponent>(entity);

            float scaleFactor = 1.0f;

            if (canvas.IsScreenSpace && canvas.ScaleWithScreenSize)
            {
                float logWidth = glm::log2((float)viewportWidth / canvas.ReferenceResolution.x);
                float logHeight = glm::log2((float)viewportHeight / canvas.ReferenceResolution.y);

                float avgLog = glm::mix(logWidth, logHeight, canvas.MatchWidthOrHeight);
                scaleFactor = glm::pow(2.0f, avgLog);
            }
            
            if (canvas.IsScreenSpace)
            {
                rect.ScreenPosition = { 0.0f, 0.0f };
                rect.ScreenSize = { (float)viewportWidth, (float)viewportHeight };
            }
            else
            {
                auto& worldTransform = m_Registry.get<TransformComponent>(entity);
                glm::vec3 worldPos = worldTransform.WorldMatrix[3];

                glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);

                if (clipPos.w <= 0.0f)
                {
                    rect.ScreenSize = glm::vec2(0.0f);
                    continue;
                }

                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                glm::vec2 screenPos;
                screenPos.x = (ndc.x + 1.0f) * 0.5f * viewportWidth;
                screenPos.y = (ndc.y + 1.0f) * 0.5f * viewportHeight;

                rect.ScreenPosition = screenPos;
                rect.ScreenSize = glm::vec2(0.0f, 0.0f);

                if (canvas.DistanceScaling)
                {
                    float dist = glm::length(worldPos - cameraPos);
                    scaleFactor = 10.0f / dist;
                }
            }

            if (relation.FirstChild != entt::null)
            {
                UpdateEntityLayout(relation.FirstChild, rect, scaleFactor);
            }
        }
    }

    void Scene::UpdateUIInteraction(float mouseX, float mouseY, bool leftButtonDown)
    {
        // TODO: We should optimize this. In the engine loop we already collect all UI Elements and sort them. Maybe we could reuse that here
        struct Interactable
        {
            entt::entity Entity;
            RectTransformComponent* Rect;
            SpriteComponent* Sprite;
            UIButtonComponent* Button;
        };
        std::vector<Interactable> interactables;

        auto view = m_Registry.view<RectTransformComponent, UIButtonComponent, SpriteComponent>();
        for (auto entity : view)
        {
            auto [rect, btn, sprite] = view.get<RectTransformComponent, UIButtonComponent, SpriteComponent>(entity);
            if (btn.Interactable)
            {
                interactables.push_back({ entity, &rect, &sprite, &btn });
            }
            else
            {
                sprite.ColorTint = btn.DisabledColor;
            }
        }

        std::sort(interactables.begin(), interactables.end(), [](const auto& a, const auto& b) {
            return a.Sprite->Layer > b.Sprite->Layer;
        });

        bool inputComsumed = false;

        for (auto& item : interactables)
        {
            item.Button->IsClicked = false;

            if (inputComsumed)
            {
                item.Button->State = UIInteractionState::Normal;
                item.Sprite->ColorTint = item.Button->NormalColor;
                continue;
            }

            bool hover = (mouseX >= item.Rect->ScreenPosition.x &&
              mouseX <= item.Rect->ScreenPosition.x + item.Rect->ScreenSize.x &&
              mouseY >= item.Rect->ScreenPosition.y &&
              mouseY <= item.Rect->ScreenPosition.y + item.Rect->ScreenSize.y);

            if (hover)
            {
                inputComsumed = true;

                if (leftButtonDown)
                {
                    item.Button->State = UIInteractionState::Pressed;
                    item.Sprite->ColorTint = item.Button->PressedColor;
                }
                else
                {
                    if (item.Button->State == UIInteractionState::Pressed)
                    {
                        item.Button->IsClicked = true;

                        if (m_Registry.all_of<NativeScriptComponent>(item.Entity))
                        {
                            auto& nsc = m_Registry.get<NativeScriptComponent>(item.Entity);
                            if (nsc.Instance)
                                nsc.Instance->OnUIPressed();
                        }

                        if (m_Registry.all_of<LuaScriptComponent>(item.Entity))
                        {
                            Entity e = { item.Entity, this };
                            Engine::Get().GetScriptEngine()->OnUIPressed(e);
                        }
                    }
                    item.Button->State = UIInteractionState::Hovered;
                    item.Sprite->ColorTint = item.Button->HoverColor;
                }
            }
            else
            {
                item.Button->State = UIInteractionState::Normal;
                item.Sprite->ColorTint = item.Button->NormalColor;
            }
        }
    }

    void Scene::UpdateEntityLayout(entt::entity entity, const RectTransformComponent& parentRect, float scaleFactor)
    {
        auto& rel = m_Registry.get<RelationshipComponent>(entity);
        
        if (!m_Registry.all_of<RectTransformComponent>(entity))
        {
            if (rel.NextSibling != entt::null)
                UpdateEntityLayout(rel.NextSibling, parentRect, scaleFactor);

            return;
        }

        auto& rect = m_Registry.get<RectTransformComponent>(entity);

        glm::vec2 parentMin = parentRect.ScreenPosition;
        glm::vec2 parentSize = parentRect.ScreenSize;

        glm::vec2 anchorMinPos = parentMin + (parentSize * rect.AnchorMin);
        glm::vec2 anchorMaxPos = parentMin + (parentSize * rect.AnchorMax);

        glm::vec2 minPoint = anchorMinPos + (rect.OffsetMin * scaleFactor);
        glm::vec2 maxPoint = anchorMaxPos + (rect.OffsetMax * scaleFactor);

        glm::vec2 rawSize = maxPoint - minPoint;
        glm::vec2 pivotPos = minPoint + (rawSize * rect.Pivot);
        glm::vec2 newSize = rawSize * rect.Scale;

        rect.ScreenPosition = pivotPos - (newSize * rect.Pivot);
        rect.ScreenSize = newSize;

        if (rel.FirstChild != entt::null)
            UpdateEntityLayout(rel.FirstChild, rect, scaleFactor);

        if (rel.NextSibling != entt::null)
            UpdateEntityLayout(rel.NextSibling, rect, scaleFactor);
    }

    void Scene::AttachEntity(entt::entity child, entt::entity parent)
    {
        DetachEntity(child);

        auto& childRel = m_Registry.get<RelationshipComponent>(child);
        childRel.Parent = parent;

        if (m_Registry.all_of<RigidBodyComponent>(child))
        {
            LX_CORE_WARN("Cannot attach an entity that has a RigidBodyComponent!");
            return;
        }

        auto& parentRel = m_Registry.get<RelationshipComponent>(parent);
        if (parentRel.FirstChild == entt::null)
        {
            parentRel.FirstChild = child;
        }
        else
        {
            entt::entity current = parentRel.FirstChild;
            while (true)
            {
                auto& currentRel = m_Registry.get<RelationshipComponent>(current);
                if (currentRel.NextSibling == entt::null)
                {
                    currentRel.NextSibling = child;
                    childRel.PrevSibling = currentRel.PrevSibling;
                    break;
                }
                current = currentRel.NextSibling;
            }
        }

        parentRel.ChildrenCount++;
    }

    void Scene::AttachEntityKeepWorld(entt::entity child, entt::entity parent)
    {
        if (m_Registry.all_of<RigidBodyComponent>(child))
        {
            LX_CORE_WARN("Cannot attach an entity that has a RigidBodyComponent!");
            return;
        }
        
        auto& childTransform = m_Registry.get<TransformComponent>(child);
        auto& parentTransform = m_Registry.get<TransformComponent>(parent);

        glm::mat4 childWorld = childTransform.WorldMatrix;
        glm::mat4 parentWorld = parentTransform.WorldMatrix;
        glm::mat4 parentInverse = glm::inverse(parentWorld);

        glm::mat4 newLocal = parentInverse * childWorld;
        SetTransformFromMatrix(childTransform, newLocal);

        AttachEntity(child, parent);
        UpdateEntityTransform(child, parentWorld);
    }

    void Scene::DetachEntity(entt::entity child)
    {
        auto& childRel = m_Registry.get<RelationshipComponent>(child);
        entt::entity parent = childRel.Parent;

        if (parent == entt::null)
            return;

        auto& parentRel = m_Registry.get<RelationshipComponent>(parent);

        if (parentRel.FirstChild == child)
        {
            parentRel.FirstChild = childRel.NextSibling;
        }

        if (childRel.PrevSibling != entt::null)
        {
            m_Registry.get<RelationshipComponent>(childRel.PrevSibling).NextSibling = childRel.NextSibling;
        }

        if (childRel.NextSibling != entt::null)
        {
            m_Registry.get<RelationshipComponent>(childRel.NextSibling).PrevSibling = childRel.PrevSibling;
        }

        childRel.Parent = entt::null;
        childRel.NextSibling = entt::null;
        childRel.PrevSibling = entt::null;

        parentRel.ChildrenCount--;
    }

    void Scene::DetachEntityKeepWorld(entt::entity child)
    {
        auto& rel = m_Registry.get<RelationshipComponent>(child);
        if (rel.Parent == entt::null)
            return;

        auto& childTransform = m_Registry.get<TransformComponent>(child);
        SetTransformFromMatrix(childTransform, childTransform.WorldMatrix);
        DetachEntity(child);
        childTransform.WorldMatrix = childTransform.GetTransform();
    }

    void Scene::UpdateGlobalTransforms()
    {
        auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
        for (auto entity : view)
        {
            const auto& rel = view.get<RelationshipComponent>(entity);
            if (rel.Parent == entt::null)
            {
                auto& transform = view.get<TransformComponent>(entity);
                transform.WorldMatrix = transform.GetTransform();

                if (rel.FirstChild != entt::null)
                {
                    UpdateEntityTransform(rel.FirstChild, transform.WorldMatrix);
                }
            }
        }
    }

    bool Scene::LoadSourceData()
    {
        SceneSerializer serializer(shared_from_this());
        return serializer.Deserialize(m_FilePath);
    }

    void Scene::UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform)
    {
        auto& transform = m_Registry.get<TransformComponent>(entity);
        auto& rel = m_Registry.get<RelationshipComponent>(entity);

        transform.WorldMatrix = parentTransform * transform.GetTransform();

        if (rel.NextSibling != entt::null)
            UpdateEntityTransform(rel.NextSibling, parentTransform);

        if (rel.FirstChild != entt::null)
            UpdateEntityTransform(rel.FirstChild, transform.WorldMatrix);
    }
}
