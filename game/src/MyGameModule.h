#pragma once
#include <Lynx.h>
#include <Lynx/GameModule.h>

struct PlayerComponent {
    float speed = 10.0f;
};

class MyGame : public Lynx::IGameModule 
{
public:
    void RegisterComponents(Lynx::ComponentRegistry* registry) override;
    void OnStart() override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
};