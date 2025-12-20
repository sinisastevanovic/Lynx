#pragma once
#include <Lynx.h>
#include <Lynx/GameModule.h>

struct PlayerComponent {
    float speed = 10.0f;
};

class MyGame : public Lynx::IGameModule
{
public:
    virtual void RegisterComponents(Lynx::ComponentRegistry* registry) override;
    virtual void OnStart() override;
    virtual void OnUpdate(float deltaTime) override;
    virtual void OnShutdown() override;
};