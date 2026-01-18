#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"

class WeaponSystem
{
public:

    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        auto& engine = Engine::Get();
        auto& assetManager = engine.GetAssetManager();

        auto sourceView = scene->Reg().view<TransformComponent, WeaponComponent>();
        for (auto entity : sourceView)
        {
            auto [sourceTransform, weapon] = sourceView.get<TransformComponent, WeaponComponent>(entity);

            if (weapon.CooldownTimer > 0.0f)
            {
                weapon.CooldownTimer -= dt;
                continue;
            }

            entt::entity target = entt::null;
            float closestDistSq = weapon.Range * weapon.Range;
            glm::vec3 targetPos = { 0, 0, 0 };

            auto targetView = scene->Reg().view<TransformComponent, EnemyComponent>();
            for (auto targetEntity: targetView)
            {
                auto& targetTransform = targetView.get<TransformComponent>(targetEntity);
                float distSq = glm::distance2(sourceTransform.Translation, targetTransform.Translation);

                if (distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    target = targetEntity;
                    targetPos = targetTransform.Translation;
                }
            }

            if (target != entt::null)
            {
                SpawnProjectile(scene, entity, assetManager, sourceTransform.Translation, targetPos, weapon);
                weapon.CooldownTimer = weapon.FireRate;
            }
        }
    }

private:
    static void SpawnProjectile(const std::shared_ptr<Scene>& scene, entt::entity owner, AssetManager& assetManager, glm::vec3 start, glm::vec3 target, const WeaponComponent& weapon)
    {
        if (weapon.ProjectilePrefab.IsValid())
        {
            auto bullet = scene->InstantiatePrefab(weapon.ProjectilePrefab);
            
            glm::vec3 direction = glm::normalize(target - start);
            auto& transform = bullet.GetComponent<TransformComponent>();
            transform.Translation = start + glm::vec3(0, 0.5f, 0);
            transform.Scale = { 0.2f, 0.2f, 0.2f };
            
            if (!bullet.HasComponent<ProjectileComponent>())
                bullet.AddComponent<ProjectileComponent>();

            auto& proj = bullet.GetComponent<ProjectileComponent>();
            proj.Damage = weapon.Damage;
            proj.Velocity = direction * weapon.ProjectileSpeed;
            proj.Owner = owner;
        }
    }
    
};
