#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"
#include "Lynx/Physics/PhysicsSystem.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class EnemySystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        // --- AI / Movement Logic
        glm::vec3 playerPos = { 0, 0, 0 };
        auto playerView = scene->View<TransformComponent, PlayerComponent>();
        for (auto pEntity : playerView)
        {
            playerPos = playerView.get<TransformComponent>(pEntity).Translation;
            break;
        }

        auto& bodyInterface = scene->GetPhysicsSystem().GetBodyInterface();
        auto enemyView = scene->View<TransformComponent, EnemyComponent, RigidBodyComponent>();
        for (auto eEntity : enemyView)
        {
            auto [transform, enemy, rb] = enemyView.get<TransformComponent, EnemyComponent, RigidBodyComponent>(eEntity);

            glm::vec3 direction = playerPos - transform.Translation;
            direction.y = 0;

            if (glm::length(direction) > 0.1f)
            {
                direction = glm::normalize(direction);

                float targetAngle = atan2(direction.x, direction.z);
                transform.Rotation = glm::angleAxis(targetAngle, glm::vec3(0, 1, 0));

                if (rb.RuntimeBodyCreated)
                {
                    JPH::Vec3 currentVel = bodyInterface.GetLinearVelocity(rb.BodyId);
                    JPH::Vec3 newVel(direction.x * enemy.MoveSpeed, currentVel.GetY(), direction.z * enemy.MoveSpeed);

                    bodyInterface.ActivateBody(rb.BodyId);
                    bodyInterface.SetLinearVelocity(rb.BodyId, newVel);

                    JPH::Quat joltRot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
                    bodyInterface.SetRotation(rb.BodyId, joltRot, JPH::EActivation::Activate);
                }
            }
        }
    }
};
