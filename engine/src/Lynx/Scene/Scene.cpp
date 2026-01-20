#include "Scene.h"
#include "Components/Components.h"
#include "Components/PhysicsComponents.h"
#include "Entity.h"
#include "Lynx/Engine.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "Components/LuaScriptComponent.h"
#include "Components/NativeScriptComponent.h"
#include "Lynx/Scripting/ScriptEngine.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <Jolt/Physics/Body/BodyInterface.h>

#include "SceneSerializer.h"
#include "Components/IDComponent.h"
#include "Components/UIComponents.h"
#include "Lynx/Asset/Prefab.h"
#include "Lynx/Event/AssetEvents.h"
#include "Lynx/Physics/PhysicsSystem.h"

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
            Scene* scene = registry.ctx().get<Scene*>();
            auto& bodyInterface = scene->GetPhysicsSystem().GetBodyInterface();

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
                Engine::Get().GetScriptEngine()->OnScriptComponentDestroyed(e);
            }
        }
    }
    
    static void OnLuaScriptComponentConstructed(entt::registry& registry, entt::entity entity)
    {
        if (Engine::Get().GetSceneState() == SceneState::Edit)
        {
            Scene* scene = Engine::Get().GetActiveScene().get();
            if (scene)
            {
                Entity e{entity, scene};
                Engine::Get().GetScriptEngine()->OnScriptComponentAdded(e);
            }
        }
    }
    
    Scene::Scene(const std::string& filePath)
        : Asset(filePath)
    {
        m_Registry.ctx().emplace<Scene*>(this);
        m_Registry.ctx().emplace<entt::dispatcher>();
        
        m_Registry.on_destroy<RigidBodyComponent>().connect<&OnRigidBodyComponentDestroyed>();
        m_Registry.on_destroy<NativeScriptComponent>().connect<&OnNativeScriptComponentDestroyed>();
        m_Registry.on_destroy<LuaScriptComponent>().connect<&OnLuaScriptComponentDestroyed>();
        
        m_PhysicsSystem = std::make_unique<PhysicsSystem>();
    }

    Scene::~Scene()
    {
        m_PhysicsSystem.reset();
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

    Entity Scene::InstantiatePrefab(std::shared_ptr<Prefab> prefab)
    {
        return InstantiatePrefab(prefab, {});
    }

    Entity Scene::InstantiatePrefab(AssetHandle prefab)
    {
        return InstantiatePrefab(prefab, {});
    }

    Entity Scene::InstantiatePrefab(std::shared_ptr<Prefab> prefab, Entity parent)
    {
        if (!prefab)
            return {};
        
        auto data = prefab->GetData();
        Entity rootInstance = SceneSerializer::DeserializePrefab(this, data, parent);
        if (rootInstance)
        {
            auto entitiesJson = data["Entities"];
            if (entitiesJson.is_array())
            {
                int jsonIndex = 0;
                std::function<void(Entity)> tagEntity = [&](Entity current)
                {
                    if (jsonIndex >= entitiesJson.size())
                        return;
                    
                    UUID originalID = entitiesJson[jsonIndex]["ID"].get<UUID>();
                    jsonIndex++;
                    
                    auto& pc = current.AddComponent<PrefabComponent>();
                    pc.Prefab = prefab;
                    pc.SubEntityID = originalID;
                    
                    auto& rel = current.GetComponent<RelationshipComponent>();
                    entt::entity child = rel.FirstChild;
                    while (child != entt::null)
                    {
                        tagEntity(Entity(child, this));
                        child = Reg().get<RelationshipComponent>(child).NextSibling;
                    }
                };
                
                tagEntity(rootInstance);
            }
        }
        return rootInstance;
    }

    Entity Scene::InstantiatePrefab(AssetHandle prefab, Entity parent)
    {
        auto prefabAsset = Engine::Get().GetAssetManager().GetAsset<Prefab>(prefab, AssetLoadMode::Blocking);
        return InstantiatePrefab(prefabAsset, parent);
    }
    
    void Scene::CreateRuntimeBody(entt::entity entity, const TransformComponent& transform, RigidBodyComponent& rigidBody)
    {
        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        JPH::ShapeRefC innerShape;
        glm::vec3 offset = { 0.0f, 0.0f, 0.0f };

        if (m_Registry.all_of<BoxColliderComponent>(entity))
        {
            auto& collider = m_Registry.get<BoxColliderComponent>(entity);
            innerShape = new JPH::BoxShape(JPH::Vec3(collider.HalfSize.x, collider.HalfSize.y, collider.HalfSize.z));
            offset = collider.Offset;
        }
        else if (m_Registry.all_of<CapsuleColliderComponent>(entity))
        {
            auto& collider = m_Registry.get<CapsuleColliderComponent>(entity);
            float halfHeight = collider.HalfHeight - collider.Radius;
            if (halfHeight <= 0.0f)
                halfHeight = 0.01f;

            innerShape = new JPH::CapsuleShape(halfHeight, collider.Radius);
            offset = collider.Offset;
        }
        else if (m_Registry.all_of<SphereColliderComponent>(entity))
        {
            auto& collider = m_Registry.get<SphereColliderComponent>(entity);
            innerShape = new JPH::SphereShape(collider.Radius);
            offset = collider.Offset;
        }
        else
        {
            LX_CORE_WARN("Entity '{0}' has Rigidbody but no Collider! Defaulting to small box.", (uint32_t)entity);
            innerShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
        }
        
        JPH::ShapeRefC finalShape = innerShape;
        if (glm::length2(offset) > 0.0001f)
        {
            JPH::RotatedTranslatedShapeSettings offsetSettings(
                JPH::Vec3(offset.x, offset.y, offset.z),
                JPH::Quat::sIdentity(),
                innerShape
            );
            
            auto result = offsetSettings.Create();
            if (result.IsValid())
                finalShape = result.Get();
            else
                LX_CORE_ERROR("Failed to create offset shape for entity {}", (uint32_t)entity);
        }
        
        // TODO: Add kinematic!
        JPH::ObjectLayer layer = (rigidBody.Type == RigidBodyType::Static) ? Layers::STATIC : Layers::MOVABLE;
        JPH::EMotionType motion = (rigidBody.Type == RigidBodyType::Static) ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;

        JPH::Vec3 pos(transform.Translation.x, transform.Translation.y, transform.Translation.z);
        JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

        JPH::BodyCreationSettings bodySettings(finalShape, pos, rot, motion, layer);

        JPH::EAllowedDOFs allowedDOFs = JPH::EAllowedDOFs::All;
        if (rigidBody.LockRotationX)
            allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationX;
        if (rigidBody.LockRotationY)
            allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationY;
        if (rigidBody.LockRotationZ)
            allowedDOFs = allowedDOFs & ~JPH::EAllowedDOFs::RotationZ;
        bodySettings.mAllowedDOFs = allowedDOFs;

        JPH::Body* body = bodyInterface.CreateBody(bodySettings);

        bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);

        rigidBody.BodyId = body->GetID();
        rigidBody.RuntimeBodyCreated = true;
    }

    void Scene::OnRuntimeStart()
    {
        Engine::Get().GetRenderer().SetShowUI(true);
        
        Engine::Get().GetScriptEngine()->OnRuntimeStart(this);
    
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
                CreateRuntimeBody(entity, transform, rb);
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

        /*auto luaView = m_Registry.view<LuaScriptComponent>();
        for (auto entity : luaView)
        {
            Entity e = { entity, this };
            Engine::Get().GetScriptEngine()->OnScriptComponentAdded(e);
        }*/
        
        UpdateGlobalTransforms();
    }

    void Scene::OnRuntimeStop()
    {
        auto luaView = m_Registry.view<LuaScriptComponent>();
        for (auto entity : luaView)
        {
            Entity e = { entity, this };
            Engine::Get().GetScriptEngine()->OnScriptComponentDestroyed(e);
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
        
        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

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
        m_PhysicsSystem->Simulate(deltaTime);
        
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
        
        JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

        // Sync physics -> transforms
        auto view = m_Registry.view<TransformComponent, RigidBodyComponent, RelationshipComponent>();
        for (auto entity : view)
        {
            auto [transform, rb, rel] = view.get<TransformComponent, RigidBodyComponent, RelationshipComponent>(entity);
            
            if (!rb.RuntimeBodyCreated)
            {
                CreateRuntimeBody(entity, transform, rb);
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

        auto& renderer = Engine::Get().GetRenderer();
        if (renderer.GetShowUI())
        {
            auto viewportSize = renderer.GetViewportSize();
            float width = (float)viewportSize.first;
            float height = (float)viewportSize.second;
            
            // TODO: Maybe don't sort every frame...
            m_Registry.sort<UICanvasComponent>([](const UICanvasComponent& lhs, const UICanvasComponent& rhs) {
                return lhs.SortingOrder < rhs.SortingOrder;
            });

            auto uiView = m_Registry.view<UICanvasComponent>(entt::exclude<DisabledComponent>);
            for (auto entity : uiView)
            {
                auto& component = uiView.get<UICanvasComponent>(entity);
                if (component.Canvas)
                {
                    component.Canvas->Update(Engine::Get().GetUnscaledDeltaTime(), width, height);
                }
            }
        }
        
        /*glm::vec3 cameraPos;
        auto camView = m_Registry.view<TransformComponent, CameraComponent>();
        for (auto entity : camView)
        {
            auto [transform, camera] = camView.get<TransformComponent, CameraComponent>(entity);
            if (camera.Primary)
            {
                cameraPos = transform.Translation; // TODO: This is wrong if the camera entity is attached to an entity 
                break;
            }
        }*/
    }

    void Scene::OnUpdateEditor(float deltaTime, glm::vec3 cameraPos)
    {
        auto& renderer = Engine::Get().GetRenderer();
        if (renderer.GetShowUI())
        {
            auto viewportSize = renderer.GetViewportSize();
            float width = (float)viewportSize.first;
            float height = (float)viewportSize.second;
            
            // TODO: Maybe don't sort every frame...
            m_Registry.sort<UICanvasComponent>([](const UICanvasComponent& lhs, const UICanvasComponent& rhs) {
                return lhs.SortingOrder < rhs.SortingOrder;
            });

            auto uiView = m_Registry.view<UICanvasComponent>();
            for (auto entity : uiView)
            {
                auto& component = uiView.get<UICanvasComponent>(entity);
                if (component.Canvas)
                {
                    component.Canvas->Update(deltaTime, width, height);
                }
            }
        }
        
        UpdateGlobalTransforms();
    }

    void Scene::OnEvent(Event& event)
    {
        auto& renderer = Engine::Get().GetRenderer();
        if (renderer.GetShowUI())
        {
            auto view = m_Registry.view<UICanvasComponent>();
            for (auto entity : view)
            {
                auto& comp = view.get<UICanvasComponent>(entity);
                if (comp.Canvas)
                    comp.Canvas->OnEvent(event);
            }
        }
        
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<AssetReloadedEvent>([this](AssetReloadedEvent& event)
        {
            auto view = m_Registry.view<PrefabComponent>();
            for (auto entityID : view)
            {
                Entity entity(entityID, this);
                auto& pc = entity.GetComponent<PrefabComponent>();
                
                if (pc.Prefab.Handle == event.GetHandle())
                {
                    Entity parent = entity.GetParent();
                    bool isRoot = true;
                    if (parent && parent.HasComponent<PrefabComponent>() && parent.GetComponent<PrefabComponent>().Prefab == pc.Prefab)
                    {
                        isRoot = false;
                    }
                    
                    if (isRoot)
                    {
                        auto prefab = Engine::Get().GetAssetManager().GetAsset<Prefab>(event.GetHandle(), AssetLoadMode::Blocking);
                        if (prefab)
                        {
                            auto json = prefab->GetData();
                            SceneSerializer::DeserializePrefabInto(this, json, entity);
                        }
                    }
                }
            }
            return false;
        });
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
                    childRel.PrevSibling = current;
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

    std::shared_ptr<class UIElement> Scene::FindUIElementByID(UUID id)
    {
        auto view = m_Registry.view<UICanvasComponent>();
        for (auto entity : view)
        {
            auto& comp = view.get<UICanvasComponent>(entity);
            if (comp.Canvas)
            {
                auto found = comp.Canvas->FindElementByID(id);
                if (found)
                    return found;
            }
        }
        return nullptr;
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

    void Scene::PostDeserialize()
    {
        m_Registry.on_construct<LuaScriptComponent>().connect<&OnLuaScriptComponentConstructed>();
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
