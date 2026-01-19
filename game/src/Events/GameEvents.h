#pragma once
#include <entt/entt.hpp>

struct LevelUpEvent
{
    entt::entity Player;
    int NewLevel;
};