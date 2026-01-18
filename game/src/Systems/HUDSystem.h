#pragma once
#include "Lynx.h"
#include "Components/GameComponents.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIText.h"

class HUDSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float deltaTime)
    {
        auto view = scene->Reg().view<PlayerHUDComponent, ExperienceComponent>();
        
        for (auto entity : view)
        {
            const auto& [hud, xp] = view.get<PlayerHUDComponent, ExperienceComponent>(entity);
            
            std::shared_ptr<UIImage> bar = hud.CachedXPBar.lock();
            if (!bar && hud.XPBarID.IsValid())
            {
                auto el = scene->FindUIElementByID(hud.XPBarID);
                bar = std::dynamic_pointer_cast<UIImage>(el);
                hud.CachedXPBar = bar;
            }
            
            if (bar)
            {
                bar->SetFillAmount(xp.CurrentXP / xp.TargetXP);
            }
            
            std::shared_ptr<UIText> text = hud.CachedLevelText.lock();
            if (!text && hud.LevelTextID.IsValid())
            {
                auto el = scene->FindUIElementByID(hud.LevelTextID);
                text = std::dynamic_pointer_cast<UIText>(el);
                hud.CachedLevelText = text;
            }
            
            if (text && xp.Level != xp.LastUILevel)
            {
                std::string label = "LVL " + std::to_string(xp.Level);
                text->SetText(label);
                xp.LastUILevel = xp.Level;
            }
        }
    }
};
