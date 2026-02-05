#include "Scene.h"
#include "Components/Components.h"
#include "Components/PhysicsComponents.h"
#include "Entity.h"
#include "Lynx/Engine.h"

#include "Components/LuaScriptComponent.h"
#include "Components/NativeScriptComponent.h"
#include "Lynx/Scripting/ScriptEngine.h"
#include <glm/gtx/matrix_decompose.hpp>

#include "SceneSerializer.h"
#include "Components/IDComponent.h"
#include "Components/UIComponents.h"
#include "Lynx/Asset/Prefab.h"
#include "Lynx/Event/AssetEvents.h"
#include "Lynx/Physics/PhysicsWorld.h"

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
            Scene* scene = registry.ctx().get<Scene*>();
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
            Scene* scene = registry.ctx().get<Scene*>();
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
        
        m_Registry.on_destroy<NativeScriptComponent>().connect<&OnNativeScriptComponentDestroyed>();
        m_Registry.on_destroy<LuaScriptComponent>().connect<&OnLuaScriptComponentDestroyed>();
        
    }

    Scene::~Scene()
    {
        m_PhysicsWorld.reset();
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        auto& idComp = entity.AddComponent<IDComponent>();
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;
        
        m_EntityMap[idComp.ID] = entity;
        Emit<EntityCreatedEvent>(entity);
        
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
        
        Entity e(entity, this);
        Emit<EntityDestroyedEvent>(e);
        auto& idComp = m_Registry.get<IDComponent>(entity);
        m_EntityMap.erase(idComp.ID);
        m_Registry.destroy(entity);
    }
    
    void Scene::DestroyEntity(Entity entity, bool excludeChildren)
    {
        DestroyEntity(entity.GetHandle(), excludeChildren);
    }

    void Scene::DestroyEntityDeferred(entt::entity entity, bool excludeChildren)
    {
        if (!m_Registry.valid(entity))
            return;
        
        if (!excludeChildren)
        {
            auto& rel = m_Registry.get<RelationshipComponent>(entity);
            entt::entity child = rel.FirstChild;
            while (child != entt::null)
            {
                entt::entity nextChild = m_Registry.get<RelationshipComponent>(child).NextSibling;
                DestroyEntityDeferred(entity, excludeChildren);
                child = nextChild;
            }
        }
        
        m_DestroyQueue.push_back(entity);
    }

    void Scene::DestroyEntityDeferred(Entity entity, bool excludeChildren)
    {
        DestroyEntityDeferred(entity.GetHandle(), excludeChildren);
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
                    
                    auto& pc = current.AddOrReplaceComponent<PrefabComponent>();
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

    Entity Scene::FindEntityByName(const std::string& name)
    {
        if (name.empty())
            return {};
        
        auto view = m_Registry.view<TagComponent>();
        for (auto entity : view)
        {
            if (view.get<TagComponent>(entity).Tag == name)
            {
                return { entity, this };
            }
        }
        
        return {};
    }

    Entity Scene::FindEntityByUUID(UUID uuid)
    {
        if (m_EntityMap.find(uuid) != m_EntityMap.end())
            return {m_EntityMap.at(uuid), this};
        
        return {};
    }

    /*void Scene::CreateRuntimeBody(entt::entity entity, const TransformComponent& transform, RigidBodyComponent& rigidBody)
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
        JPH::ObjectLayer Layer = (rigidBody.Type == RigidBodyType::Static) ? Layers::STATIC : Layers::MOVABLE;
        JPH::EMotionType motion = (rigidBody.Type == RigidBodyType::Static) ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;

        JPH::Vec3 pos(transform.Translation.x, transform.Translation.y, transform.Translation.z);
        JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

        JPH::BodyCreationSettings bodySettings(finalShape, pos, rot, motion, Layer);
        bodySettings.mUserData = (uint64_t)entity;

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

    void Scene::CreateRuntimeCharacter(entt::entity entity, const TransformComponent& transform, struct CharacterControllerComponent& character)
    {
        JPH::ShapeRefC innerShape;
        float offsetY = 0.0f;
        
        if (m_Registry.all_of<CapsuleColliderComponent>(entity))
        {
            auto& collider = m_Registry.get<CapsuleColliderComponent>(entity);
            float effectiveRadius = collider.Radius - character.CharacterPadding;
            float halfHeight = collider.HalfHeight - collider.Radius;
            if (halfHeight <= 0.0f)
                halfHeight = 0.01f;
            
            innerShape = new JPH::CapsuleShape(halfHeight, effectiveRadius);
            offsetY = collider.Offset.y;
        }
        else
        {
            LX_CORE_WARN("CharacterController requires a CapsuleCollider! Defaulting.");
            innerShape = new JPH::CapsuleShape(0.5f, 0.5f);
        }
        
        JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
        settings->mShape = innerShape;
        settings->mMaxSlopeAngle = glm::radians(character.MaxSlopeAngle);
        settings->mMaxStrength = character.MaxStrength;
        settings->mCharacterPadding = character.CharacterPadding;
       // settings->mStepHeight = character.StepHeight;
        settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -1.0f * character.CharacterPadding); // TODO:??
        
        JPH::Vec3 position(transform.Translation.x, transform.Translation.y + offsetY, transform.Translation.z);
        JPH::Quat rotation(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
        
        auto& physSystem = m_PhysicsSystem->GetJoltSystem();
        
        character.Character = new JPH::CharacterVirtual(settings, position, rotation, (uint64_t)entity, &physSystem); // TODO: When to delete?
        character.RuntimeCreated = true;
    }*/

    void Scene::ProcessDestroyQueue()
    {
        for (auto entity : m_DestroyQueue)
        {
            Entity e(entity, this);
            Emit<EntityDestroyedEvent>(e);
            auto& idComp = m_Registry.get<IDComponent>(entity);
            m_EntityMap.erase(idComp.ID);
            m_Registry.destroy(entity);
        }
        m_DestroyQueue.clear();
    }

    void Scene::OnRuntimeStart()
    {
        m_PhysicsWorld = std::make_unique<PhysicsWorld>();
        
        m_GameSystems.Init(*this);
        m_PhysicsSystem.OnInit(*this);
        
        Engine::Get().GetRenderer().SetShowUI(true);
        
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
        
        m_GameSystems.SceneStart(*this);
        m_PhysicsSystem.OnSceneStart(*this);
        
        UpdateGlobalTransforms();
    }

    void Scene::OnRuntimeStop()
    {
        m_GameSystems.SceneStop(*this);
        
        auto luaView = m_Registry.view<LuaScriptComponent>();
        for (auto entity : luaView)
        {
            Entity e = { entity, this };
            Engine::Get().GetScriptEngine()->OnScriptComponentDestroyed(e);
        }

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
        
        m_PhysicsSystem.OnShutdown(*this); // TODO: Do the bodies get destroyed correctly??
        m_GameSystems.Shutdown(*this);
        
        m_PhysicsWorld.reset();
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
        
        m_GameSystems.Update(*this, deltaTime);
    }

    void Scene::OnFixedUpdate(float fixedDeltaTime)
    {
        m_GameSystems.FixedUpdate(*this, fixedDeltaTime);
        m_PhysicsSystem.OnFixedUpdate(*this, fixedDeltaTime);
    }

    void Scene::OnLateUpdate(float deltaTime)
    {
        m_GameSystems.LateUpdate(*this, deltaTime);
        
        UpdateGlobalTransforms();
        ProcessDestroyQueue();
        DispatchEvents();
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
