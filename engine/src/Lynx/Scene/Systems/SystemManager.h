#pragma once

#include "ISystem.h"

namespace Lynx
{
    class SystemManager
    {
    public:
        template <typename T, typename... Args>
        T* AddSystem(Args&&... args)
        {
            static_assert(std::is_base_of_v<ISystem, T>);
            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T* ptr = system.get();
            m_Systems.push_back(std::move(system));
            return ptr;
        }
        
        template <typename T>
        T* GetSystem()
        {
            for (auto& sys : m_Systems)
            {
                if (auto* casted = dynamic_cast<T*>(sys.get()))
                    return casted;
            }
            return nullptr;
        }
        
        template <typename T>
        bool RemoveSystem()
        {
            auto it = std::find_if(m_Systems.begin(), m_Systems.end(), [](const auto& sys)
            {
                return dynamic_cast<T*>(sys.get()) != nullptr;
            });
            if (it != m_Systems.end())
            {
                m_Systems.erase(it);
                return true;
            }
            return false;
        }
        
        void Init(Scene& scene);
        void Shutdown(Scene& scene);
        void SceneStart(Scene& scene);
        void SceneStop(Scene& scene);
        void Update(Scene& scene, float deltaTime);
        void FixedUpdate(Scene& scene, float fixedDeltaTime);
        void LateUpdate(Scene& scene, float deltaTime);
        
        const auto& GetSystems() { return m_Systems; }
    
    private:
        std::vector<std::unique_ptr<ISystem>> m_Systems;
    };

}
