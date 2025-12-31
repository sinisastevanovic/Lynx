#pragma once
#include <memory>

#include "Components/GameComponents.h"
#include "Lynx/Engine.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Components/Components.h"

class WeaponSystem
{
public:

    static void Update(std::shared_ptr<Lynx::Scene> scene, float dt)
    {
        auto& engine = Lynx::Engine::Get();
        auto& assetManager = engine.GetAssetManager();

        auto sourceView = scene->Reg().view<Lynx::TransformComponent, WeaponComponent>();
        for (auto entity : sourceView)
        {
            auto [sourceTransform, weapon] = sourceView.get<Lynx::TransformComponent, WeaponComponent>(entity);

            if (weapon.CooldownTimer > 0.0f)
            {
                weapon.CooldownTimer -= dt;
                continue;
            }

            entt::entity target = entt::null;
            float closestDistSq = weapon.Range * weapon.Range;
            glm::vec3 targetPos = { 0, 0, 0 };

            auto targetView = scene->Reg().view<Lynx::TransformComponent, EnemyComponent>();
            for (auto targetEntity: targetView)
            {
                auto& targetTransform = targetView.get<Lynx::TransformComponent>(targetEntity);
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
                SpawnProjectile(scene, assetManager, sourceTransform.Translation, targetPos, weapon);
                weapon.CooldownTimer = weapon.FireRate;
            }
        }
    }

private:
    static void SpawnProjectile(const std::shared_ptr<Lynx::Scene>& scene, Lynx::AssetManager& assetManager, glm::vec3 start, glm::vec3 target, const WeaponComponent& weapon)
    {
        glm::vec3 direction = glm::normalize(target - start);

        auto bullet = scene->CreateEntity("Bullet");
        auto& transform = bullet.GetComponent<Lynx::TransformComponent>();
        transform.Translation = start + glm::vec3(0, 0.5f, 0);
        transform.Scale = { 0.2f, 0.2f, 0.2f };

        auto& mesh = bullet.AddComponent<Lynx::MeshComponent>();
        mesh.Mesh = assetManager.GetDefaultCube()->GetHandle();

        auto& proj = bullet.AddComponent<ProjectileComponent>();
        proj.Damage = weapon.Damage;
        proj.Velocity = direction * weapon.ProjectileSpeed;
        proj.Lifetime = 3.0f;
    }
    
};
