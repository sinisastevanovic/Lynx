#include "PhysicsSystem.h"

#include "PhysicsEvents.h"
#include "PhysicsWorld.h"
#include "Lynx/Asset/Terrain.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"
#include "Lynx/Scene/Components/TerrainComponent.h"

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

        // The ground is there from the start, for rays shot before the first step.
        UpdateTerrainColliders(scene);
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

        // The bodies themselves go with the physics world.
        m_TerrainBodies.clear();
        m_Scene = nullptr;
    }

    void PhysicsSystem::OnFixedUpdate(Scene& scene, float fixedDeltaTime)
    {
        auto& physics = scene.GetPhysicsWorldChecked();

        CreatePendingRigidBodies(scene);
        UpdateTerrainColliders(scene);

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
        desc.Position = transform.GetTranslation(); // TODO: Use world!
        desc.Rotation = transform.GetRotation();
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

    void PhysicsSystem::CreatePendingRigidBodies(Scene& scene)
    {
        auto& registry = scene.Reg();

        // TryCreateRigidBody removes an entity from the set once its body exists, so the loop runs over a copy.
        const std::vector<entt::entity> pending(m_PendingRigidBodies.begin(), m_PendingRigidBodies.end());
        for (entt::entity entity : pending)
        {
            // An entity that was destroyed, or a collider without a rigid body, has nothing to wait for.
            // Adding a rigid body later puts the entity back into the set.
            if (!registry.valid(entity) || !registry.all_of<RigidBodyComponent>(entity))
            {
                m_PendingRigidBodies.erase(entity);
                continue;
            }

            TryCreateRigidBody(scene, entity);
        }
    }

    void PhysicsSystem::UpdateTerrainColliders(Scene& scene)
    {
        auto& registry = scene.Reg();
        auto& physics = scene.GetPhysicsWorldChecked();

        const auto destroy = [&physics](TerrainBodies& terrain)
        {
            for (BodyId body : terrain.Bodies)
                physics.DestroyBody(body);
            terrain.Bodies.clear();
        };

        // Nothing is listened for: the few terrains of a scene are looked at before every step, which
        // also catches a terrain that was reloaded or moved.
        for (auto it = m_TerrainBodies.begin(); it != m_TerrainBodies.end();)
        {
            if (registry.valid(it->first) && registry.all_of<TransformComponent, TerrainComponent, TerrainColliderComponent>(it->first))
            {
                ++it;
                continue;
            }
            destroy(it->second);
            it = m_TerrainBodies.erase(it);
        }

        const auto terrains = registry.view<TransformComponent, TerrainComponent, TerrainColliderComponent>();
        for (auto entity : terrains)
        {
            auto [transform, terrain, collider] = terrains.get<TransformComponent, TerrainComponent, TerrainColliderComponent>(entity);
            const std::shared_ptr<Terrain> asset = terrain.Terrain.Get();
            TerrainBodies& made = m_TerrainBodies[entity];
            if (!asset || asset->GetTilesPerSide() == 0)
            {
                destroy(made);
                made = {};
                continue;
            }

            // Like the renderer, physics takes the entity's position as the terrain's first corner
            // and nothing else of its transform.
            const glm::vec3 origin(transform.GetWorldMatrix()[3]);
            const bool isCurrent = !made.Bodies.empty() && made.Terrain == asset->GetHandle() && made.Version == asset->GetVersion()
                && made.Origin == origin && made.Friction == collider.Friction && made.Restitution == collider.Restitution;
            if (isCurrent)
                continue;

            destroy(made);
            made.Terrain = asset->GetHandle();
            made.Version = asset->GetVersion();
            made.Origin = origin;
            made.Friction = collider.Friction;
            made.Restitution = collider.Restitution;

            // One height field per tile, from the heights the tile is drawn from. Neighbours share
            // their row of heights along an edge, so the ground has no step there.
            const uint32_t quads = asset->GetTileQuads();
            const uint32_t samples = quads + 1;
            for (uint32_t tileZ = 0; tileZ < asset->GetTilesPerSide(); ++tileZ)
            {
                for (uint32_t tileX = 0; tileX < asset->GetTilesPerSide(); ++tileX)
                {
                    HeightFieldShape shape;
                    shape.SampleCount = samples;
                    shape.Spacing = asset->GetSpacing();
                    shape.Heights.resize(static_cast<size_t>(samples) * samples);
                    for (uint32_t z = 0; z < samples; ++z)
                    {
                        for (uint32_t x = 0; x < samples; ++x)
                            shape.Heights[static_cast<size_t>(z) * samples + x] = asset->GetHeightAt(tileX * quads + x, tileZ * quads + z);
                    }

                    BodyDescriptor desc;
                    desc.Shape = std::move(shape);
                    desc.Position = origin + glm::vec3(static_cast<float>(tileX), 0.0f, static_cast<float>(tileZ)) * asset->GetTileSize();
                    desc.Type = BodyType::Static;
                    desc.Layer = CollisionLayer::Static;
                    desc.Friction = collider.Friction;
                    desc.Restitution = collider.Restitution;
                    desc.Entity = entity;

                    const BodyId body = physics.CreateBody(desc);
                    if (body != INVALID_BODY)
                        made.Bodies.push_back(body);
                }
            }
        }
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
        desc.Position = transform.GetTranslation();
        desc.Rotation = transform.GetRotation();
        desc.CapsuleHalfHeight = cc.CapsuleHalfHeight;
        desc.CapsuleRadius = cc.CapsuleRadius;
        desc.Mass = cc.Mass;
        desc.MaxSlopeAngle = glm::radians(cc.MaxSlopeAngle); // The component holds degrees.
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
            
            transform.SetTranslation(physics.GetCharacterPosition(cc.CharacterId));
            transform.SetRotation(physics.GetCharacterRotation(cc.CharacterId));
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
                physics.MoveKinematic(rb.BodyId, transform.GetTranslation(), transform.GetRotation(), fixedDeltaTime);
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
                transform.SetTranslation(physics.GetPosition(rb.BodyId));
                transform.SetRotation(physics.GetRotation(rb.BodyId));
            }
        }
        
        auto charView = registry.view<TransformComponent, CharacterControllerComponent>();
        for (auto [entity, transform, cc] : charView.each())
        {
            if (cc.CharacterId == INVALID_CHARACTER)
                continue;
            
            // Note: Previous transform is saved in SavePhysicsState before the update
            
            transform.SetTranslation(physics.GetCharacterPosition(cc.CharacterId));
            transform.SetRotation(physics.GetCharacterRotation(cc.CharacterId));
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
            transform.PreviousTranslation = transform.GetTranslation();
            transform.PreviousRotation = transform.GetRotation();
        }
    }
}
