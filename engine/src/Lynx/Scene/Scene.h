#pragma once

#include "Lynx/Core.h"
#include <entt/entt.hpp>

namespace Lynx
{
    class Entity;

    class LX_API Scene
    {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(entt::entity entity);

        void OnRuntimeStart();
        void OnRuntimeStop();

        void OnUpdateRuntime(float deltaTime);
        void OnUpdateEditor(float deltaTime);

        // Temporary access to registry
        entt::registry& Reg() { return m_Registry; }

    private:
        entt::registry m_Registry;

        friend class Entity;
        friend class SceneHierarchyPanel; // For editor later
    };
}
