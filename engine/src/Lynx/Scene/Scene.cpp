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
    namespace
    {
        bool IsSameOrAncestorOf(entt::registry& registry, entt::entity candidate, entt::entity entity)
        {
            for (entt::entity current = entity; current != entt::null; current = registry.get<RelationshipComponent>(current).Parent)
            {
                if (current == candidate)
                    return true;
            }
            return false;
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

        // A transform that is new, or was written to as a whole: by loading, by a script through its
        // fields, by the inspector. It is told whom it belongs to and put on the list of changed ones.
        // Its own setters put it on the list themselves.
        m_Registry.on_construct<TransformComponent>().connect<&Scene::OnTransformWritten>(this);
        m_Registry.on_update<TransformComponent>().connect<&Scene::OnTransformWritten>(this);

        // A mesh component is told whom it belongs to as well, so that its setters can announce a change.
        m_Registry.on_construct<MeshComponent>().connect<&Scene::OnMeshWritten>(this);
        m_Registry.on_update<MeshComponent>().connect<&Scene::OnMeshWritten>(this);
        
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

        // The children are handled when the queue is processed, by the same code as an immediate destroy.
        m_DestroyQueue.push_back({ entity, excludeChildren });
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

        JPH::Vec3 pos(transform.GetTranslation().x, transform.GetTranslation().y, transform.GetTranslation().z);
        JPH::Quat rot(transform.GetRotation().x, transform.GetRotation().y, transform.GetRotation().z, transform.GetRotation().w);

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
        
        JPH::Vec3 position(transform.GetTranslation().x, transform.GetTranslation().y + offsetY, transform.GetTranslation().z);
        JPH::Quat rotation(transform.GetRotation().x, transform.GetRotation().y, transform.GetRotation().z, transform.GetRotation().w);
        
        auto& physSystem = m_PhysicsSystem->GetJoltSystem();
        
        character.Character = new JPH::CharacterVirtual(settings, position, rotation, (uint64_t)entity, &physSystem); // TODO: When to delete?
        character.RuntimeCreated = true;
    }*/

    void Scene::ProcessDestroyQueue()
    {
        // Destroying an entity emits an event, and a listener may queue further entities.
        // Those go into a fresh queue, for the next call.
        const std::vector<PendingDestroy> queue = std::move(m_DestroyQueue);
        m_DestroyQueue.clear();

        // DestroyEntity ignores an entity that is already gone, such as one queued twice
        // or a child that went with its parent earlier in this loop.
        for (const PendingDestroy& pending : queue)
            DestroyEntity(pending.Entity, pending.ExcludeChildren);
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

        // Physics places what it makes by where things are in the world.
        UpdateGlobalTransforms();
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
                cameraPos = transform.GetTranslation(); // TODO: This is wrong if the camera entity is attached to an entity 
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

        // After the transforms, so particles are emitted where their emitter is in this frame.
        m_ParticleSystem.Simulate(deltaTime, *this);
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
        m_ParticleSystem.Simulate(deltaTime, *this);
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
        // Every check comes before the first change, so a refused attach leaves the hierarchy as it was.
        if (m_Registry.all_of<RigidBodyComponent>(child))
        {
            LX_CORE_WARN("Cannot attach an entity that has a RigidBodyComponent!");
            return;
        }

        if (IsSameOrAncestorOf(m_Registry, child, parent))
        {
            LX_CORE_WARN("Cannot attach an entity to itself or to one of its own descendants.");
            return;
        }

        DetachEntity(child);

        auto& childRel = m_Registry.get<RelationshipComponent>(child);
        childRel.Parent = parent;

        // Under another parent the same position means another place in the world.
        TransformComponent& childTransform = m_Registry.get<TransformComponent>(child);
        MarkTransformChanged(child, childTransform);
        childTransform.m_HasRelatives = true;
        m_Registry.get<TransformComponent>(parent).m_HasRelatives = true;

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

        // The place in the world stays, so the place relative to the new parent is worked out from it.
        SetLocalFromMatrix(child, glm::inverse(parentTransform.m_WorldMatrix) * childTransform.m_WorldMatrix);
        AttachEntity(child, parent);
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

        TransformComponent& childTransform = m_Registry.get<TransformComponent>(child);
        MarkTransformChanged(child, childTransform);
        childTransform.m_HasRelatives = childRel.FirstChild != entt::null;
        m_Registry.get<TransformComponent>(parent).m_HasRelatives = parentRel.Parent != entt::null || parentRel.FirstChild != entt::null;

        parentRel.ChildrenCount--;
    }

    void Scene::DetachEntityKeepWorld(entt::entity child)
    {
        auto& rel = m_Registry.get<RelationshipComponent>(child);
        if (rel.Parent == entt::null)
            return;

        // Without a parent, the place in the world is the place.
        SetLocalFromMatrix(child, m_Registry.get<TransformComponent>(child).m_WorldMatrix);
        DetachEntity(child);
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

    void Scene::SetLocalFromMatrix(entt::entity entity, const glm::mat4& matrix)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(matrix, scale, rotation, translation, skew, perspective);
        m_Registry.get<TransformComponent>(entity).SetTransform(translation, rotation, scale);
    }

    void TransformComponent::NotifyScene()
    {
        m_Scene->MarkTransformChanged(m_Entity, *this);
    }

    void MeshComponent::NotifyScene()
    {
        if (m_Scene)
            m_Scene->m_Registry.patch<MeshComponent>(m_Entity);
    }

    void Scene::MarkTransformChanged(entt::entity entity, TransformComponent& transform)
    {
        if (transform.m_IsChanged)
            return;

        transform.m_IsChanged = true;
        m_ChangedTransforms.push_back(entity);
    }

    void Scene::OnTransformWritten(entt::registry& registry, entt::entity entity)
    {
        // Put on the list whatever the mark says: a transform that was replaced as a whole may have
        // lost it, or have it without being on the list.
        TransformComponent& transform = registry.get<TransformComponent>(entity);
        transform.m_Scene = this;
        transform.m_Entity = entity;
        transform.m_IsChanged = true;
        m_ChangedTransforms.push_back(entity);
    }

    void Scene::OnMeshWritten(entt::registry& registry, entt::entity entity)
    {
        MeshComponent& mesh = registry.get<MeshComponent>(entity);
        mesh.m_Scene = this;
        mesh.m_Entity = entity;
    }

    void Scene::UpdateGlobalTransforms()
    {
        // The list is taken over before it is worked through, so that it is free for what is written next.
        m_ChangedTransformsInWork.swap(m_ChangedTransforms);

        auto& transforms = m_Registry.storage<TransformComponent>();
        for (const entt::entity entity : m_ChangedTransformsInWork)
        {
            // An entity that was destroyed since is no longer among the transforms.
            if (!transforms.contains(entity))
                continue;

            // Without the mark it is done already: as the child of an entity earlier in the list, or
            // because it is in the list twice.
            TransformComponent& transform = transforms.get(entity);
            if (!transform.m_IsChanged)
                continue;

            // Most entities stand alone. Their place relative to a parent is their place in the world.
            if (!transform.m_HasRelatives)
            {
                transform.m_IsChanged = false;
                SetWorldMatrix(entity, transform, transform.GetTransform());
                continue;
            }

            // A parent that is later in the list has not moved yet. The child is then worked out twice,
            // the second time when the parent is, which is right in the end.
            const entt::entity parent = m_Registry.get<RelationshipComponent>(entity).Parent;
            UpdateWorldMatrices(entity, transform, parent != entt::null ? &transforms.get(parent).m_WorldMatrix : nullptr);
        }

        m_ChangedTransformsInWork.clear();
    }

    bool Scene::SetWorldMatrix(entt::entity entity, TransformComponent& transform, const glm::mat4& worldMatrix)
    {
        // A write does not always move the entity in the world. Compared as bytes: that is one
        // comparison of 64 bytes where comparing the numbers is sixteen.
        if (std::memcmp(&transform.m_WorldMatrix, &worldMatrix, sizeof(glm::mat4)) == 0)
            return false;

        transform.m_WorldMatrix = worldMatrix;
        m_MovedEntities.push_back(entity);
        return true;
    }

    void Scene::UpdateWorldMatrices(entt::entity entity, TransformComponent& transform, const glm::mat4* parentWorld)
    {
        transform.m_IsChanged = false;

        // If the entity stays where it is in the world, so do its children.
        const glm::mat4 local = transform.GetTransform();
        if (!SetWorldMatrix(entity, transform, parentWorld ? *parentWorld * local : local))
            return;

        // The children move with it. The depth of the calls is the depth of the hierarchy.
        entt::entity child = m_Registry.get<RelationshipComponent>(entity).FirstChild;
        while (child != entt::null)
        {
            UpdateWorldMatrices(child, m_Registry.get<TransformComponent>(child), &transform.m_WorldMatrix);
            child = m_Registry.get<RelationshipComponent>(child).NextSibling;
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
}
