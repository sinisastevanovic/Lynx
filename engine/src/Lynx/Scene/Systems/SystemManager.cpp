#include "SystemManager.h"

namespace Lynx
{
    void SystemManager::Init(Scene& scene)
    {
        for (auto& system : m_Systems)
        {
            system->OnInit(scene);
        }
    }

    void SystemManager::Shutdown(Scene& scene)
    {
        for (auto& system : m_Systems)
        {
            system->OnShutdown(scene);
        }
    }

    void SystemManager::SceneStart(Scene& scene)
    {
        for (auto& system : m_Systems)
        {
            system->OnSceneStart(scene);
        }
    }

    void SystemManager::SceneStop(Scene& scene)
    {
        for (auto& system : m_Systems)
        {
            system->OnSceneStop(scene);
        }
    }

    void SystemManager::Update(Scene& scene, float deltaTime)
    {
        for (auto& system : m_Systems)
        {
            system->OnUpdate(scene, deltaTime);
        }
    }

    void SystemManager::FixedUpdate(Scene& scene, float fixedDeltaTime)
    {
        for (auto& system : m_Systems)
        {
            system->OnFixedUpdate(scene, fixedDeltaTime);
        }
    }

    void SystemManager::LateUpdate(Scene& scene, float deltaTime)
    {
        for (auto& system : m_Systems)
        {
            system->OnLateUpdate(scene, deltaTime);
        }
    }
}
