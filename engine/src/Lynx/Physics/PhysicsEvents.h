#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Lynx
{
    struct CollisionEnterEvent
    {
        entt::entity EntityA;
        entt::entity EntityB;
        glm::vec3 ContactPoint;
        glm::vec3 ContactNormal;
        float PenetrationDepth;
    };
    
    struct CollisionStayEvent
    {
        entt::entity EntityA;
        entt::entity EntityB;
        glm::vec3 ContactPoint;
        glm::vec3 ContactNormal;
        float PenetrationDepth;
    };
    
    struct CollisionExitEvent
    {
        entt::entity EntityA;
        entt::entity EntityB;
    };
    
    struct TriggerEnterEvent
    {
        entt::entity EntityA;
        entt::entity EntityB;
        glm::vec3 ContactPoint;
        glm::vec3 ContactNormal;
    };
    
    struct TriggerStayEvent
    {
        entt::entity EntityA;
        entt::entity EntityB;
    };
    
    struct TriggerExitEvent
    {
        entt::entity EntityA;
        entt::entity EntityB;
    };
    
    struct CharacterCollisionEvent
    {
        entt::entity CharacterEntity;
        entt::entity OtherEntity;
        glm::vec3 ContactPoint;
        glm::vec3 ContactNormal;
    };
}
