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

            auto targetView = scene->Reg().view<TransformComponent, EnemyComponent>();
            for (auto targetEntity : targetView)
            {
                auto& targetTransform = targetView.get<TransformComponent>(targetEntity);

                float distSq = glm::distance2(transform.Translation, targetTransform.Translation);
                float hitRadius = 0.5f + projectile.Radius;

                if (distSq < (hitRadius * hitRadius))
                {
                    auto& enemy = targetView.get<EnemyComponent>(targetEntity);
                    enemy.Health -= projectile.Damage;

                    if (enemy.Health <= 0.0f)
                    {
                        scene->DestroyEntity(targetEntity);

                        if (projectile.Owner != entt::null && scene->Reg().valid(projectile.Owner))
                        {
                            if (scene->Reg().all_of<ExperienceComponent>(projectile.Owner))
                            {
                                auto& xpComp = scene->Reg().get<ExperienceComponent>(projectile.Owner);
                                xpComp.CurrentXP += 10.0f; // TODO: Don't hardcode and also enemies should drop XP Orbs
                            }
                        }
                    }

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
