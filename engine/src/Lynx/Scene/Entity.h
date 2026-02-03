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
            : m_Handle(handle), m_Scene(scene) {}
        Entity(const Entity& other) = default;

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            LX_ASSERT(!HasComponent<T>(), "Entity already has component!");
            return m_Scene->Reg().emplace<T>(m_Handle, std::forward<Args>(args)...);
        }
        
        template<typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... args)
        {
            return m_Scene->Reg().emplace_or_replace<T>(m_Handle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent()
        {
            LX_ASSERT(HasComponent<T>(), "Entity does not have component!");
            return m_Scene->Reg().get<T>(m_Handle);
        }
        
        template<typename T>
        const T& GetComponent() const
        {
            LX_ASSERT(HasComponent<T>(), "Entity does not have component!");
            return m_Scene->Reg().get<T>(m_Handle);
        }
        
        template<typename T>
        T* TryGetComponent()
        {
            return m_Scene->Reg().try_get<T>(m_Handle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return m_Scene->Reg().all_of<T>(m_Handle);
        }

        template<typename T>
        void RemoveComponent()
        {
            m_Scene->Reg().remove<T>(m_Handle);
        }

        void AttachEntity(Entity parent, bool keepWorld = true)
        {
            if (keepWorld)
                m_Scene->AttachEntityKeepWorld(m_Handle, parent);
            else
                m_Scene->AttachEntity(m_Handle, parent);
        }

        void DetachEntity(bool keepWorld = true)
        {
            if (keepWorld)
                m_Scene->DetachEntityKeepWorld(m_Handle);
            else
                m_Scene->DetachEntity(m_Handle);
        }
        
        bool HasParent() const;
        Entity GetParent() const;
        bool HasChild() const;
        
        void SetVisibility(bool visible);
        
        Scene* GetScene() const
        {
            return m_Scene;
        }
        
        entt::entity GetHandle() const
        {
            return m_Handle;
        }

        UUID GetUUID() const;
        
        bool IsValid() const
        {
            return m_Handle != entt::null && m_Scene != nullptr && m_Scene->Reg().valid(m_Handle);
        }

        operator bool() const { return IsValid(); }
        operator entt::entity() const { return m_Handle; }
        operator uint32_t() const { return (uint32_t)m_Handle; }

        bool operator==(const Entity& other) const
        {
            return m_Handle == other.m_Handle && m_Scene == other.m_Scene;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }

    private:
        entt::entity m_Handle{ entt::null };
        Scene* m_Scene = nullptr;
    };
    
    struct EntityCreatedEvent { Entity entity; };
    struct EntityDestroyedEvent { Entity entity; };
}
