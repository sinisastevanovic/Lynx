#pragma once
#include "Lynx.h"
#include "Components/GameComponents.h"

class PickupSystem
{
public:
    // TODO: Shouldnt this be events? I mean this could listen to a enemy dead event or so...
    static void SpawnPickup(std::shared_ptr<Scene> scene, std::shared_ptr<Prefab> prefab, glm::vec3 position, float amount)
    {
        auto entity = scene->InstantiatePrefab(prefab);
        
        auto& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = position;
        // TODO: multiply scale by the amount! So more XP->Bigger Orb
        
        auto& pickup = entity.GetComponent<PickupComponent>();
        pickup.Value = amount;
    }
    
    static void Update(std::shared_ptr<Scene> scene, float deltaTime)
    {
        auto& reg = scene->Reg();
        
        auto playerView = reg.view<TransformComponent, MagnetComponent, ExperienceComponent, HealthComponent, CharacterStatsComponent>();
        auto pickupView = reg.view<TransformComponent, PickupComponent>();
        
        for (auto playerEntity : playerView)
        {
            auto [pTrans, magnet, xpComp, hpComp, stats] = playerView.get(playerEntity);
            glm::vec3 pPos = pTrans.GetWorldTranslation();
            
            for (auto pickupEntity : pickupView)
            {
                auto [pickupTrans, pickup] = pickupView.get(pickupEntity);
                
                glm::vec3 pickupPos = pickupTrans.GetWorldTranslation();
                float distSq = glm::distance2(pPos, pickupPos);
                if (pickup.Magnetic && ((pickup.IsMagnetized && pickup.MagentizedBy == playerEntity) || (!pickup.IsMagnetized && distSq < (stats.MagnetRadius * stats.MagnetRadius))))
                {
                    pickup.IsMagnetized = true;
                    pickup.MagentizedBy = playerEntity;
                    glm::vec3 dir = glm::normalize(pPos - pickupPos);
                    pickupTrans.Translation += dir * magnet.Strength * deltaTime;
                }
                
                if (distSq < 1.0f)
                {
                    switch (pickup.Type)
                    {
                        case PickupType::XP:
                            xpComp.CurrentXP += pickup.Value;
                            break;
                        case PickupType::Health:
                            hpComp.CurrentHealth = glm::min(hpComp.CurrentHealth + pickup.Value, hpComp.MaxHealth);
                            break;
                    }
                        
                    scene->DestroyEntityDeferred(pickupEntity);
                }
            }
        }
    }
};
