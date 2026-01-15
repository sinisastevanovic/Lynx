#include "Entity.h"
#include "Components/Components.h"
#include "Lynx/Scene/Components/IDComponent.h"

namespace Lynx
{
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
            m_Scene->m_Registry.remove<DisabledComponent>(m_EntityHandle);
        else if (!visible && !HasComponent<DisabledComponent>())
            m_Scene->m_Registry.emplace<DisabledComponent>(m_EntityHandle);
    }

    UUID Entity::GetUUID() const
    {
        return GetComponent<IDComponent>().ID;
    }
}
