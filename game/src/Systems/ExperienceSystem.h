#pragma once

#include "Lynx.h"
#include "Components/GameComponents.h"
#include "Events/GameEvents.h"

class ExperienceSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float deltaTime)
    {
        auto view = scene->View<ExperienceComponent>();
        for (auto entity : view)
        {
            auto& xp = view.get<ExperienceComponent>(entity);

            while (xp.CurrentXP >= xp.TargetXP)
            {
                xp.CurrentXP -= xp.TargetXP;
                xp.Level++;
                xp.TargetXP *= 1.2f; // TODO: Use actual curve
                
                Entity e{entity, scene.get()};
                scene->Emit<LevelUpEvent>(entity, xp.Level);
                Engine::Get().GetScriptEngine()->OnGlobalEvent(scene.get(), "OnLevelUp", e, xp.Level); // TODO: Script code should use the event dispatcher too!
            }
        }
    }
};
