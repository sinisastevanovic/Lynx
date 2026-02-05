#include "Entity.h"
#include "Scene.h"
#include "Components/Components.h"
#include "Lynx/Scene/Components/IDComponent.h"

namespace Lynx
{
    Entity::Entity(entt::entity handle, Scene* scene)
        : m_Handle(handle), m_Scene(scene), m_Registry(&m_Scene->Reg())
    {
    }

    void Entity::AttachEntity(Entity parent, bool keepWorld)
    {
        if (keepWorld)
            m_Scene->AttachEntityKeepWorld(m_Handle, parent);
        else
            m_Scene->AttachEntity(m_Handle, parent);
    }

    void Entity::DetachEntity(bool keepWorld)
    {
        if (keepWorld)
            m_Scene->DetachEntityKeepWorld(m_Handle);
        else
            m_Scene->DetachEntity(m_Handle);
    }

    bool Entity::HasParent() const
    {
        return GetComponent<RelationshipComponent>().Parent != entt::null;
    }

    Entity Entity::GetParent() const
    {
        Entity result = {};
        const auto relComp = GetComponent<RelationshipComponent>();
        if (relComp.Parent != entt::null)
        {
            result = { relComp.Parent, m_Scene };
        }
        return result;
    }

    bool Entity::HasChild() const
    {
        return GetComponent<RelationshipComponent>().FirstChild != entt::null;
    }

    void Entity::SetVisibility(bool visible)
    {
        if (visible && HasComponent<DisabledComponent>())
            m_Scene->Reg().remove<DisabledComponent>(m_Handle);
        else if (!visible && !HasComponent<DisabledComponent>())
            m_Scene->Reg().emplace<DisabledComponent>(m_Handle);
    }

    UUID Entity::GetUUID() const
    {
        return GetComponent<IDComponent>().ID;
    }
}
