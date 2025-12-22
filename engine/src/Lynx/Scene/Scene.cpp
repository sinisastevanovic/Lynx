#include "Scene.h"
#include "Components/Components.h"
#include "Components/PhysicsComponents.h"
#include "Entity.h"
#include "Lynx/Engine.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace Lynx
{
    static void OnRigidBodyComponentDestroyed(entt::registry& registry, entt::entity entity)
    {
        auto& rb = registry.get<RigidBodyComponent>(entity);
        if (rb.RuntimeBodyCreated)
        {
            auto& physicsSystem = Engine::Get().GetPhysicsSystem();
            auto& bodyInterface = physicsSystem.GetBodyInterface();

            bodyInterface.RemoveBody(rb.BodyId);
            bodyInterface.DestroyBody(rb.BodyId);

            LX_CORE_TRACE("Physics body destroyed for entity {0}", (uint32_t)entity);
        }
    }
    
    Scene::Scene()
    {
        m_Registry.on_destroy<RigidBodyComponent>().connect<&OnRigidBodyComponentDestroyed>();
    }

    Scene::~Scene()
    {
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>();
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::DestroyEntity(entt::entity entity)
    {
        m_Registry.destroy(entity);
    }

    void Scene::OnRuntimeStart()
    {
        auto& physicsSystem = Engine::Get().GetPhysicsSystem();
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

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
                    float halfHeight = (collider.Height - (collider.Radius * 2.0f)) * 0.5f;
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
    }

    void Scene::OnRuntimeStop()
    {
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
        auto& physicsSystem = Engine::Get().GetPhysicsSystem();
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

        // Sync physics -> transforms
        auto view = m_Registry.view<TransformComponent, RigidBodyComponent>();
        for (auto entity : view)
        {
            auto [transform, rb] = view.get<TransformComponent, RigidBodyComponent>(entity);
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
}
