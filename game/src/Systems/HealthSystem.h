#pragma once

#include "Lynx.h"
#include "Components/GameComponents.h"

class HealthSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene)
    {
        auto view = scene->Reg().view<HealthComponent>(entt::exclude<DeadTag>);
        for (auto entity : view)
        {
            auto& health = view.get<HealthComponent>(entity);
            if (health.CurrentHealth <= 0.0f)
            {
                scene->Reg().emplace<DeadTag>(entity);
            }
        }
    }
};
