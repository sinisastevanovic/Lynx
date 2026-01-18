#pragma once

#include "Lynx.h"
#include "Components/GameComponents.h"

class ExperienceSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float deltaTime)
    {
        auto view = scene->Reg().view<ExperienceComponent>();
        for (auto entity : view)
        {
            auto& xp = view.get<ExperienceComponent>(entity);

            while (xp.CurrentXP >= xp.TargetXP)
            {
                xp.CurrentXP -= xp.TargetXP;
                xp.Level++;
                xp.TargetXP *= 1.2f; // TODO: Use actual curve

                // TODO: emit level up event and show upgrades etc
            }
        }
    }
};
