#pragma once
#include "Lynx.h"
#include "WaveSystem.h"
#include "Components/GameComponents.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIText.h"

class HUDSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float deltaTime)
    {
        {
            auto view = scene->View<PlayerHUDComponent, ExperienceComponent>();
            for (auto entity : view)
            {
                const auto& [hud, xp] = view.get<PlayerHUDComponent, ExperienceComponent>(entity);
            
                if (hud.XPBar)
                {
                    hud.XPBar.Get(scene.get())->SetFillAmount(xp.CurrentXP / xp.TargetXP);
                }
            
                if (hud.LevelText && xp.Level != xp.LastUILevel)
                {
                    std::string label = "LVL " + std::to_string(xp.Level);
                    hud.LevelText.Get(scene.get())->SetText(label);
                    xp.LastUILevel = xp.Level;
                }
            }
        }
        
        {
            auto view = scene->View<PlayerHUDComponent, HealthComponent>();
            for (auto entity : view)
            {
                const auto& [hud, hp] = view.get<PlayerHUDComponent, HealthComponent>(entity);
            
                if (hud.HPBar)
                {
                    hud.HPBar.Get(scene.get())->SetFillAmount(hp.CurrentHealth / hp.MaxHealth);
                }
            
                std::string label = std::to_string(hp.CurrentHealth) + "/" + std::to_string(hp.MaxHealth);
                if (hud.HPText && hud.HPText.Get(scene.get())->GetText() != label)
                {
                    hud.HPText.Get(scene.get())->SetText(label);
                }
            }
        }
        
        {
            /*auto view = scene->View<WaveManagerComponent>();
            for (auto entity : view)
            {
                const auto& wave = view.get<WaveManagerComponent>(entity);
            
                if (wave.GameTimerText)
                {
                    hud.XPBar->SetFillAmount(hp.CurrentHealth / hp.MaxHealth);
                }
            
                std::string label = std::to_string(hp.CurrentHealth) + "/" + std::to_string(hp.MaxHealth);
                if (hud.HPText && hud.HPText->GetText() != label)
                {
                    hud.LevelText->SetText(label);
                }
            }*/
        }
    }
};
