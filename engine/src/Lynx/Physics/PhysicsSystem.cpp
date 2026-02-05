#include "PhysicsSystem.h"

#include "PhysicsEvents.h"
#include "PhysicsWorld.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

namespace Lynx
{
    void PhysicsSystem::OnInit(Scene& scene)
    {
        m_Scene = &scene;
        auto& registry = scene.Reg();
        
        registry.on_construct<RigidBodyComponent>().connect<&PhysicsSystem::OnRigidBodyAdded>(this);
        registry.on_destroy<RigidBodyComponent>().connect<&PhysicsSystem::OnRigidBodyRemoved>(this);
        
        registry.on_construct<CharacterControllerComponent>().connect<&PhysicsSystem::OnCharacterControllerAdded>(this);
        registry.on_destroy<CharacterControllerComponent>().connect<&PhysicsSystem::OnCharacterControllerRemoved>(this);
        
        registry.on_construct<BoxColliderComponent>().connect<&PhysicsSystem::OnBoxColliderAdded>(this);
        registry.on_construct<SphereColliderComponent>().connect<&PhysicsSystem::OnSphereColliderAdded>(this);
        registry.on_construct<CapsuleColliderComponent>().connect<&PhysicsSystem::OnCapsuleColliderAdded>(this);
        registry.on_construct<MeshColliderComponent>().connect<&PhysicsSystem::OnMeshColliderAdded>(this);
    }

    void PhysicsSystem::OnSceneStart(Scene& scene)
    {
        auto& registry = scene.Reg();
        auto view = registry.view<RigidBodyComponent>();
        for (auto entity : view)
        {
            m_PendingRigidBodies.insert(entity);
            TryCreateRigidBody(*m_Scene, entity);
        }
        
        auto charView = registry.view<CharacterControllerComponent>();
        for (auto entity : charView)
        {
            CreateCharacterController(*m_Scene, entity);
        }
    }

    void PhysicsSystem::OnShutdown(Scene& scene)
    {
        auto& registry = scene.Reg();

        registry.on_construct<RigidBodyComponent>().disconnect(this);
        registry.on_destroy<RigidBodyComponent>().disconnect(this);
        registry.on_construct<CharacterControllerComponent>().disconnect(this);
        registry.on_destroy<CharacterControllerComponent>().disconnect(this);
        registry.on_construct<BoxColliderComponent>().disconnect(this);
        registry.on_construct<SphereColliderComponent>().disconnect(this);
        registry.on_construct<CapsuleColliderComponent>().disconnect(this);
        registry.on_construct<MeshColliderComponent>().disconnect(this);

        m_Scene = nullptr;
    }

    void PhysicsSystem::OnFixedUpdate(Scene& scene, float fixedDeltaTime)
    {
        auto& physics = scene.GetPhysicsWorldChecked();
        
        for (BodyId id : m_BodiesToDestroy)
        {
            physics.DestroyBody(id);
        }
        m_BodiesToDestroy.clear();
        
        for (CharacterId id : m_CharactersToDestroy)
        {
            physics.DestroyCharacter(id);
        }
        m_CharactersToDestroy.clear();
        
        // Sync kinematic bodies TO physics
        SyncTransformsToPhysics(scene, fixedDeltaTime);
        
        SavePhysicsState(scene);
        
        // Update character controllers (before physics step)
        UpdateCharacters(scene, fixedDeltaTime);
        
        physics.Step(fixedDeltaTime);
        
        SyncTransformsFromPhysics(scene);
        
        ProcessCollisionEvents(scene);
    }

    void PhysicsSystem::OnRigidBodyAdded(entt::registry& registry, entt::entity entity)
    {
        m_PendingRigidBodies.insert(entity);
        //TryCreateRigidBody(*m_Scene, entity);
    }

    void PhysicsSystem::OnRigidBodyRemoved(entt::registry& registry, entt::entity entity)
    {
        m_PendingRigidBodies.erase(entity);
        
        auto& rb = registry.get<RigidBodyComponent>(entity);
        if (rb.BodyId != INVALID_BODY)
        {
            m_BodiesToDestroy.push_back(rb.BodyId);
        }
    }

    void PhysicsSystem::OnCharacterControllerAdded(entt::registry& registry, entt::entity entity)
    {
        CreateCharacterController(*m_Scene, entity);
    }

    void PhysicsSystem::OnCharacterControllerRemoved(entt::registry& registry, entt::entity entity)
    {
        auto& cc = registry.get<CharacterControllerComponent>(entity);
        if (cc.CharacterId != INVALID_CHARACTER)
        {
            m_CharactersToDestroy.push_back(cc.CharacterId);
        }
    }

    void PhysicsSystem::OnBoxColliderAdded(entt::registry& registry, entt::entity entity)
    {
        m_PendingRigidBodies.insert(entity);
        //TryCreateRigidBody(*m_Scene, entity);
    }

    void PhysicsSystem::OnSphereColliderAdded(entt::registry& registry, entt::entity entity)
    {
        m_PendingRigidBodies.insert(entity);
        //TryCreateRigidBody(*m_Scene, entity);
    }

    void PhysicsSystem::OnCapsuleColliderAdded(entt::registry& registry, entt::entity entity)
    {
        m_PendingRigidBodies.insert(entity);
        //TryCreateRigidBody(*m_Scene, entity);
    }

    void PhysicsSystem::OnMeshColliderAdded(entt::registry& registry, entt::entity entity)
    {
        m_PendingRigidBodies.insert(entity);
        //TryCreateRigidBody(*m_Scene, entity);
    }

    void PhysicsSystem::TryCreateRigidBody(Scene& scene, entt::entity entity)
    {
        auto& registry = scene.Reg();
        
        if (!m_PendingRigidBodies.contains(entity))
            return;
        if (!registry.all_of<RigidBodyComponent, TransformComponent>(entity))
            return;
        
        auto& rb = registry.get<RigidBodyComponent>(entity);
        auto& transform = registry.get<TransformComponent>(entity);
        
        if (rb.BodyId != INVALID_BODY)
            return;
        
        std::optional<CollisionShape> shape;
        bool isTrigger = false;
        
        if (auto* box = registry.try_get<BoxColliderComponent>(entity))
        {
            shape = BoxShape{box->HalfSize};
            isTrigger = box->IsTrigger;
        }
        else if (auto* sphere = registry.try_get<SphereColliderComponent>(entity))
        {
            shape = SphereShape{sphere->Radius};
            isTrigger = sphere->IsTrigger;
        }
        else if (auto* capsule = registry.try_get<CapsuleColliderComponent>(entity))
        {
            shape = CapsuleShape{capsule->Radius, capsule->HalfHeight};
            isTrigger = capsule->IsTrigger;
        }
        else if (auto* mesh = registry.try_get<MeshColliderComponent>(entity))
        {
            // TODO:!!
            isTrigger = mesh->IsTrigger;
        }
        
        // No collider yet, stay pending
        if (!shape.has_value())
            return;
        
        BodyDescriptor desc;
        desc.Shape = shape.value();
        desc.Position = transform.Translation; // TODO: Use world!
        desc.Rotation = transform.Rotation;
        desc.Type = rb.Type;
        desc.Mass = rb.Mass;
        desc.Friction = rb.Friction;
        desc.Restitution = rb.Restitution;
        desc.LinearDamping = rb.LinearDamping;
        desc.AngularDamping = rb.AngularDamping;
        desc.GravityFactor = rb.GravityFactor;
        desc.MotionQuality = rb.MotionQuality;
        desc.Layer = rb.Layer;
        switch (rb.Type)
        {
            // TODO: this is temporary
            case BodyType::Static:
                desc.Layer = CollisionLayer::Static;
                break;
            case BodyType::Dynamic:
                desc.Layer = CollisionLayer::Dynamic;
                break;
            case BodyType::Kinematic:
                desc.Layer = CollisionLayer::Dynamic;
                break;
        }
        desc.IsTrigger = isTrigger;
        desc.Entity = entity;
        
        rb.BodyId = scene.GetPhysicsWorldChecked().CreateBody(desc);
        m_PendingRigidBodies.erase(entity);
    }

    void PhysicsSystem::CreateCharacterController(Scene& scene, entt::entity entity)
    {
        auto& registry = scene.Reg();
        
        if (!registry.all_of<CharacterControllerComponent>(entity))
            return;
        
        auto& cc = registry.get<CharacterControllerComponent>(entity);
        auto& transform = registry.get<TransformComponent>(entity);
        
        if (cc.CharacterId != INVALID_CHARACTER)
            return;
        
        CharacterDescriptor desc;
        desc.Position = transform.Translation;
        desc.Rotation = transform.Rotation;
        desc.CapsuleHalfHeight = cc.CapsuleHalfHeight;
        desc.CapsuleRadius = cc.CapsuleRadius;
        desc.Mass = cc.Mass;
        desc.MaxSlopeAngle = cc.MaxSlopeAngle;
        desc.MaxStrength = cc.MaxStrength;
        desc.Layer = cc.Layer;
        desc.Entity = entity;
        //desc.PredicitveContactDistance = TODO: ??
        //desc.PenetrationRecoverySpeed = 
        
        cc.CharacterId = scene.GetPhysicsWorldChecked().CreateCharacter(desc);
    }

    void PhysicsSystem::UpdateCharacters(Scene& scene, float fixedDt)
    {
        auto& registry = scene.Reg();
        auto& physics = scene.GetPhysicsWorldChecked();
        glm::vec3 gravity = physics.GetGravity();
        
        auto view = registry.view<TransformComponent, CharacterControllerComponent>();
        for (auto [entity, transform, cc] : view.each())
        {
            if (cc.CharacterId == INVALID_CHARACTER)
                continue;
            
            physics.UpdateCharacter(cc.CharacterId, fixedDt, cc.DesiredVelocity);
            
            /*// Get current state
            glm::vec3 currentVelocity = physics.GetCharacterLinearVelocity(cc.CharacterId);
            GroundState groundState = physics.GetCharacterGroundState(cc.CharacterId);
            glm::vec3 groundVelocity = physics.GetCharacterGroundVelocity(cc.CharacterId);
            
            // Build desired velocity
            glm::vec3 desiredVelocity = cc.DesiredVelocity;
            
            // Handle vertical velocity (gravity + jumping)
            if (groundState == GroundState::Grounded || groundState == GroundState::OnSteepGround)
            {
                desiredVelocity.y = -0.5f;
                desiredVelocity += groundVelocity;
            }
            else
            {
                desiredVelocity.y = currentVelocity.y + gravity.y * fixedDt;
                desiredVelocity.x = glm::mix(currentVelocity.x, cc.DesiredVelocity.x, cc.AirControlFactor);
                desiredVelocity.z = glm::mix(currentVelocity.z, cc.DesiredVelocity.z, cc.AirControlFactor);
            }
            
            physics.UpdateCharacter(cc.CharacterId, fixedDt, desiredVelocity);*/
            
            cc.Velocity = physics.GetCharacterLinearVelocity(cc.CharacterId);
            cc.GroundState = physics.GetCharacterGroundState(cc.CharacterId);
            cc.GroundNormal = physics.GetCharacterGroundNormal(cc.CharacterId);
            cc.GroundVelocity = physics.GetCharacterGroundVelocity(cc.CharacterId);
            
            transform.Translation = physics.GetCharacterPosition(cc.CharacterId);
            transform.Rotation = physics.GetCharacterRotation(cc.CharacterId);
        }
    }

    void PhysicsSystem::SyncTransformsToPhysics(Scene& scene, float fixedDeltaTime)
    {
        auto& registry = scene.Reg();
        auto& physics = scene.GetPhysicsWorldChecked();
        
        auto view = registry.view<TransformComponent, RigidBodyComponent>();
        for (auto [entity, transform, rb] : view.each())
        {
            if (rb.BodyId == INVALID_BODY)
                continue;
            
            if (rb.Type == BodyType::Kinematic)
            {
                physics.MoveKinematic(rb.BodyId, transform.Translation, transform.Rotation, fixedDeltaTime);
            }
        }
        
        /*auto charView = registry.view<TransformComponent, CharacterControllerComponent>();
        for (auto [entity, transform, cc] : charView.each())
        {
            if (cc.CharacterId == INVALID_CHARACTER)
                continue;
            
            physics
        }*/
    }

    void PhysicsSystem::SyncTransformsFromPhysics(Scene& scene)
    {
        auto& registry = scene.Reg();
        auto& physics = scene.GetPhysicsWorldChecked();
        
        auto view = registry.view<TransformComponent, RigidBodyComponent>();
        for (auto [entity, transform, rb] : view.each())
        {
            if (rb.BodyId == INVALID_BODY)
                continue;
            
            if (rb.Type == BodyType::Dynamic)
            {
                transform.Translation = physics.GetPosition(rb.BodyId);
                transform.Rotation = physics.GetRotation(rb.BodyId);
            }
        }
        
        auto charView = registry.view<TransformComponent, CharacterControllerComponent>();
        for (auto [entity, transform, cc] : charView.each())
        {
            if (cc.CharacterId == INVALID_CHARACTER)
                continue;
            
            // Note: Previous transform is saved in SavePhysicsState before the update
            
            transform.Translation = physics.GetCharacterPosition(cc.CharacterId);
            transform.Rotation = physics.GetCharacterRotation(cc.CharacterId);
        }
    }

    void PhysicsSystem::ProcessCollisionEvents(Scene& scene)
    {
        auto& physics = scene.GetPhysicsWorldChecked();
        
        for (const CollisionEvent& event : physics.GetCollisionEnterEvents())
        {
            if (event.IsTrigger)
                scene.Emit<TriggerEnterEvent>(event.EntityA, event.EntityB, event.ContactPoint, event.ContactNormal);
            else
                scene.Emit<CollisionEnterEvent>(event.EntityA, event.EntityB, event.ContactPoint, event.ContactNormal, event.PenetrationDepth);
        }
        
        for (const CollisionEvent& event : physics.GetCollisionStayEvents())
        {
            if (event.IsTrigger)
                scene.Emit<TriggerStayEvent>(event.EntityA, event.EntityB);
            else
                scene.Emit<CollisionStayEvent>(event.EntityA, event.EntityB, event.ContactPoint, event.ContactNormal, event.PenetrationDepth);
        }
        
        for (const CollisionEvent& event : physics.GetCollisionExitEvents())
        {
            if (event.IsTrigger)
                scene.Emit<TriggerExitEvent>(event.EntityA, event.EntityB);
            else
                scene.Emit<CollisionExitEvent>(event.EntityA, event.EntityB);
        }
        
        for (const CharacterCollision& event : physics.GetCharacterCollisions()) 
        {
            scene.Emit<CharacterCollisionEvent>(
                event.CharacterEntity,
                event.OtherEntity,
                event.ContactPoint,
                event.ContactNormal
            );
        }
    }

    void PhysicsSystem::SavePhysicsState(Scene& scene)
    {
        auto& registry = scene.Reg();
        
        // Save state for ALL transforms to ensure interpolation works for everything (including Cameras)
        auto view = registry.view<TransformComponent>();
        for (auto entity : view)
        {
            auto& transform = view.get<TransformComponent>(entity);
            transform.PreviousTranslation = transform.Translation;
            transform.PreviousRotation = transform.Rotation;
        }
    }
}
