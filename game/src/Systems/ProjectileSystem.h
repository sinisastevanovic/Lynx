#pragma once
#include "Lynx.h"

#include "DamageSystem.h"
#include "Components/GameComponents.h"

class ProjectileSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        auto bulletView = scene->Reg().view<TransformComponent, ProjectileComponent>();

        std::vector<entt::entity> bulletsToDestroy;
        for (auto entity : bulletView)
        {
            auto [transform, projectile] = bulletView.get<TransformComponent, ProjectileComponent>(entity);

            transform.Translation += projectile.Velocity * dt;
            projectile.Lifetime -= dt;
            if (projectile.Lifetime <= 0.0f)
            {
                bulletsToDestroy.push_back(entity);
                continue;
            }

            auto targetView = scene->Reg().view<TransformComponent, EnemyComponent, HealthComponent>(entt::exclude<DeadTag>);
            for (auto targetEntity : targetView)
            {
                auto& targetTransform = targetView.get<TransformComponent>(targetEntity);
                glm::vec3 targetPosition = targetTransform.GetWorldTranslation();

                float distSq = glm::distance2(transform.Translation, targetPosition);
                float hitRadius = 0.5f + projectile.Radius;

                if (distSq < (hitRadius * hitRadius))
                {
                    auto& enemy = targetView.get<EnemyComponent>(targetEntity);
                    auto& targetHealth = targetView.get<HealthComponent>(targetEntity);
                    
                    targetHealth.CurrentHealth -= projectile.Damage;

                    DamageTextSystem::Spawn(targetTransform.Translation, (int)(projectile.Damage));

                    bulletsToDestroy.push_back(entity);
                    break;
                }
            }
        }

        for (auto e : bulletsToDestroy)
        {
            if (scene->Reg().valid(e))
                scene->DestroyEntity(e);
        }
    }
};
