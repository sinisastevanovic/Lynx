#include "Scene.h"
#include "Components/Components.h"
#include "Components/PhysicsComponents.h"
#include "Entity.h"
#include "Lynx/Engine.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace Lynx
{
    Scene::Scene()
    {
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

    void Scene::OnUpdate(float deltaTime)
    {
        auto& physicsSystem = Engine::Get().GetPhysicsSystem();
        JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();
        
        // Scripts, Physics, etc. would go here
        {
            // TODO: This should happen in OnRuntimeStart, we don't have that yet, so we do it here.
            // Now even in editor physics get simulated on start of the app..
            auto view = m_Registry.view<TransformComponent, RigidBodyComponent, BoxColliderComponent>();
            for (auto entity : view)
            {
                auto [transform, rb, collider] = view.get<TransformComponent, RigidBodyComponent, BoxColliderComponent>(entity);

                
                if (!rb.RuntimeBodyCreated)
                {
                    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(collider.HalfSize.x, collider.HalfSize.y, collider.HalfSize.z));

                    // TODO: Add kinematic!
                    JPH::ObjectLayer layer = (rb.Type == RigidBodyType::Static) ? Layers::STATIC : Layers::MOVABLE;
                    JPH::EMotionType motion = (rb.Type == RigidBodyType::Static) ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;

                    JPH::Vec3 pos(transform.Translation.x, transform.Translation.y, transform.Translation.z);
                    JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

                    JPH::BodyCreationSettings bodySettings(shapeSettings.Create().Get(), pos, rot, motion, layer);

                    JPH::Body* body = bodyInterface.CreateBody(bodySettings);

                    bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);

                    rb.BodyId = body->GetID();
                    rb.RuntimeBodyCreated = true;
                }
            }
        }

        // Update physics sim
        {
            auto view = m_Registry.view<TransformComponent, RigidBodyComponent>();
            for (auto entity : view)
            {
                auto [transform, rb] = view.get<TransformComponent, RigidBodyComponent>(entity);
                if (rb.RuntimeBodyCreated && rb.Type == RigidBodyType::Dynamic)
                {
                    JPH::Vec3 pos = bodyInterface.GetPosition(rb.BodyId);
                    JPH::Quat rot = bodyInterface.GetRotation(rb.BodyId);

                    transform.Translation = { pos.GetX(), pos.GetY(), pos.GetZ() };
                    transform.Rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
                }
            }
        }
        
    }
}
