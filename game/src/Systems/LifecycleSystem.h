#pragma once

#include "GameConfig.h"
#include "Lynx.h"
#include "PickupSystem.h"
#include "Lynx/Asset/Prefab.h"
#include "Components/GameComponents.h"

class LifecycleSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene)
    {
        auto& reg = scene->Reg();
        
        auto deadEnemyView = reg.view<TransformComponent, EnemyComponent, DeadTag>();
        for (auto entity : deadEnemyView)
        {
            auto [trans, enemy] = deadEnemyView.get(entity);
            if (GameConfig::XPOrbPrefab)
            {
                PickupSystem::SpawnPickup(scene, GameConfig::XPOrbPrefab, trans.GetWorldTranslation(), enemy.XPValue);
            }
        }
        
        auto deadView = reg.view<DeadTag>();
        for (auto entity : deadView)
        {
            scene->DestroyEntity(entity);
        }
    }
};
