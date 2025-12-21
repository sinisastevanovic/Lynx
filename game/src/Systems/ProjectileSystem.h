#pragma once
#include <memory>

#include "Components/GameComponents.h"
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Components/Components.h"

class ProjectileSystem
{
public:
    static void Update(std::shared_ptr<Lynx::Scene> scene, float dt)
    {
        auto bulletView = scene->Reg().view<Lynx::TransformComponent, ProjectileComponent>();

        std::vector<entt::entity> bulletsToDestroy;
        for (auto entity : bulletView)
        {
            auto [transform, projectile] = bulletView.get<Lynx::TransformComponent, ProjectileComponent>(entity);

            transform.Translation += projectile.Velocity * dt;
            projectile.Lifetime -= dt;
            if (projectile.Lifetime <= 0.0f)
            {
                bulletsToDestroy.push_back(entity);
                continue;
            }

            auto targetView = scene->Reg().view<Lynx::TransformComponent, EnemyComponent>();
            for (auto targetEntity : targetView)
            {
                auto& targetTransform = targetView.get<Lynx::TransformComponent>(targetEntity);

                float distSq = glm::distance2(transform.Translation, targetTransform.Translation);
                float hitRadius = 0.5f + projectile.Radius;

                if (distSq < (hitRadius * hitRadius))
                {
                    auto& enemy = targetView.get<EnemyComponent>(targetEntity);
                    enemy.Health -= projectile.Damage;

                    if (enemy.Health <= 0.0f)
                    {
                        scene->DestroyEntity(targetEntity);
                    }

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
