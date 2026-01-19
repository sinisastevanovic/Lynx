#pragma once

#include "Lynx.h"
#include "Components/GameComponents.h"
#include "Events/GameEvents.h"

class GameManagerSystem
{
public:
    static void Init(std::shared_ptr<Scene> scene)
    {
        auto& dispatcher = scene->Reg().ctx().get<entt::dispatcher>();
        dispatcher.sink<LevelUpEvent>().connect<&OnLevelUp>();
    }
    
    static void OnLevelUp(const LevelUpEvent& event)
    {
        LX_INFO("Player {0} leveled up to {1}!", (uint32_t)event.Player, event.NewLevel);
        
        Engine::Get().SetPaused(true);
        
        auto scene = Engine::Get().GetActiveScene();
        auto view = scene->Reg().view<PlayerHUDComponent>();
        for (auto entity : view)
        {
            if (entity == event.Player)
            {
                auto& hud = view.get<PlayerHUDComponent>(entity);
                if (hud.LevelUpCanvas)
                {
                    hud.LevelUpCanvas.Get(scene.get())->SetVisibility(UIVisibility::Visible);
                }
                break;
            }
        }
    }
    
    static void CloseUpgradeScreen()
    {
        auto scene = Engine::Get().GetActiveScene();
        auto view = scene->Reg().view<PlayerHUDComponent>();
        for (auto entity : view) // TODO: This currently closes the popup for all players. 
        {
            auto& hud = view.get<PlayerHUDComponent>(entity);
            if (hud.LevelUpCanvas)
            {
                hud.LevelUpCanvas.Get(scene.get())->SetVisibility(UIVisibility::Hidden);
            }
        }
        
        Engine::Get().SetPaused(false);
    }
};