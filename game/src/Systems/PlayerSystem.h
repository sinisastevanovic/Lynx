#pragma once
#include <memory>

#include "Components/GameComponents.h"
#include "Lynx/Engine.h"
#include "Lynx/Input.h"
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class PlayerSystem
{
public:
    static void Update(std::shared_ptr<Lynx::Scene> scene, float dt)
    {
        auto& engine = Lynx::Engine::Get();
        auto& bodyInterface = engine.GetPhysicsSystem().GetBodyInterface();
        auto view = scene->Reg().view<Lynx::TransformComponent, PlayerComponent, Lynx::RigidBodyComponent>();
        for (auto entity : view)
        {
            auto [transform, player, rb] = view.get<Lynx::TransformComponent, PlayerComponent, Lynx::RigidBodyComponent>(entity);

            glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };

            if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::W)) velocity.z -= 1.0f;
            if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::S)) velocity.z += 1.0f;
            if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::A)) velocity.x -= 1.0f;
            if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::D)) velocity.x += 1.0f;

            if (glm::length(velocity) > 0.0f)
            {
                velocity = glm::normalize(velocity) * player.MoveSpeed;

                float targetAngle = atan2(velocity.x, velocity.z);
                transform.Rotation = glm::angleAxis(targetAngle, glm::vec3(0, 1, 0));
            }

            if (rb.RuntimeBodyCreated)
            {
                JPH::BodyID bodyID = rb.BodyId;
                JPH::Vec3 currentVel = bodyInterface.GetLinearVelocity(bodyID);

                JPH::Vec3 newVel(velocity.x, currentVel.GetY(), velocity.z);
                JPH::Quat joltRot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

                bodyInterface.ActivateBody(bodyID);
                bodyInterface.SetLinearVelocity(bodyID, newVel);
                bodyInterface.SetRotation(rb.BodyId, joltRot, JPH::EActivation::Activate);
            }
        }
    }
};
