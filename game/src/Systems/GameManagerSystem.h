#pragma once

#include "Lynx.h"

class GameManagerSystem
{
public:
    static void Init(std::shared_ptr<Scene> scene)
    {
        /*auto& dispatcher = scene->Reg().ctx().get<entt::dispatcher>();
        dispatcher.sink<LevelUpEvent>().connect<&OnLevelUp>();*/
    }
};
