#pragma once

#include "Scene.h"
#include "entt/entt.hpp"
#include "Lynx/Log.h"

namespace Lynx
{
    class LX_API Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene)
            : m_EntityHandle(handle), m_Scene(scene) {}
        Entity(const Entity& other) = default;

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            if (HasComponent<T>())
            {
                LX_CORE_WARN("Entity already has component!");
                return GetComponent<T>();
            }
            return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent()
        {
            if (!HasComponent<T>())
            {
                LX_CORE_ERROR("Entity does not have component!");
                // Throw or handle?
            }
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }
        
        template<typename T>
        const T& GetComponent() const
        {
            if (!HasComponent<T>())
            {
                LX_CORE_ERROR("Entity does not have component!");
                // Throw or handle?
            }
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        template<typename T>
        void RemoveComponent()
        {
            if (!HasComponent<T>())
            {
                LX_CORE_WARN("Entity does not have component!");
                return;
            }
            m_Scene->m_Registry.remove<T>(m_EntityHandle);
        }

        void AttachEntity(Entity parent, bool keepWorld = true)
        {
            if (keepWorld)
                m_Scene->AttachEntityKeepWorld(m_EntityHandle, parent);
            else
                m_Scene->AttachEntity(m_EntityHandle, parent);
        }

        void DetachEntity(bool keepWorld = true)
        {
            if (keepWorld)
                m_Scene->DetachEntityKeepWorld(m_EntityHandle);
            else
                m_Scene->DetachEntity(m_EntityHandle);
        }
        
        bool HasParent() const;
        Entity GetParent() const;
        bool HasChild() const;
        
        void SetVisibility(bool visible);
        
        Scene* GetScene() const
        {
            return m_Scene;
        }

        UUID GetUUID() const;

        operator bool() const { return m_EntityHandle != entt::null; }
        operator entt::entity() const { return m_EntityHandle; }
        operator uint32_t() const { return (uint32_t)m_EntityHandle; }

        bool operator==(const Entity& other) const
        {
            return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }

    private:
        entt::entity m_EntityHandle{ entt::null };
        Scene* m_Scene = nullptr;
    };
}
