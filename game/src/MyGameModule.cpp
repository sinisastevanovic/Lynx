#include "MyGameModule.h"

#include <Lynx/ComponentRegistry.h>

void MyGame::RegisterComponents(Lynx::ComponentRegistry* registry)
{
    LX_INFO("Registering custom types...");
    registry->RegisterComponent<PlayerComponent>("Player Component");
}

void MyGame::OnStart()
{
    LX_INFO("OnStart called.");
}

void MyGame::OnUpdate(float deltaTime)
{
    LX_INFO("OnUpdate called.");
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
}