#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"
#include "Lynx/Physics/PhysicsWorld.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class EnemySystem : public ISystem
{
public:
    std::string GetName() const override { return "EnemySystem"; }

    void OnFixedUpdate(Scene& scene, float fixedDeltaTime) override
    {
        // --- AI / Movement Logic
        glm::vec3 playerPos = { 0, 0, 0 };
        auto playerView = scene.View<TransformComponent, PlayerComponent>();
        for (auto pEntity : playerView)
        {
            playerPos = playerView.get<TransformComponent>(pEntity).Translation;
            break;
        }

        auto& physics = scene.GetPhysicsWorldChecked();
        auto enemyView = scene.View<TransformComponent, EnemyComponent, RigidBodyComponent>();
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

                // For Kinematic bodies, we MUST update the transform.Translation.
                // The PhysicsSystem will then sync this to the physics body.
                transform.Translation += direction * enemy.MoveSpeed * fixedDeltaTime;
                
                // Optional: We can still update physics rotation directly for better responsiveness 
                // but SyncTransformsToPhysics will do it anyway.
                physics.Activate(rb.BodyId);
            }
        }
    }
};