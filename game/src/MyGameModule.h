#pragma once
#include <Lynx.h>
#include <Lynx/GameModule.h>

class MyGame : public IGameModule
{
public:
    virtual void RegisterScripts(GameTypeRegistry* registry) override;
    virtual void RegisterComponents(GameTypeRegistry* registry) override;
    virtual void OnStart() override;
    virtual void OnUpdate(float deltaTime) override;
    virtual void OnShutdown() override;
    
private:
    std::shared_ptr<Material> m_ParticleMat;
};