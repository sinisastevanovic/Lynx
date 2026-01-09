#pragma once
#include <Lynx.h>
#include <Lynx/GameModule.h>

class MyGame : public Lynx::IGameModule
{
public:
    virtual void RegisterScripts(Lynx::GameTypeRegistry* registry) override;
    virtual void RegisterComponents(Lynx::GameTypeRegistry* registry) override;
    virtual void OnStart() override;
    virtual void OnUpdate(float deltaTime) override;
    virtual void OnShutdown() override;
    
private:
    std::shared_ptr<Lynx::Material> m_ParticleMat;
};