#pragma once

namespace Lynx
{
    class LX_API ISystem
    {
    public:
        virtual ~ISystem() = default;
        
        virtual std::string GetName() const = 0;
        bool IsEnabled() const { return m_Enabled; }
        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        
        virtual void OnInit(Scene& scene) {}
        virtual void OnShutdown(Scene& scene) {}
        virtual void OnSceneStart(Scene& scene) {}
        virtual void OnSceneStop(Scene& scene) {}
        
        virtual void OnUpdate(Scene& scene, float deltaTime) {}
        virtual void OnFixedUpdate(Scene& scene, float fixedDeltaTime) {}
        virtual void OnLateUpdate(Scene& scene, float deltaTime) {}
        
    protected:
        bool m_Enabled = true;
    };
}
