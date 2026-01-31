#pragma once

#include "Lynx/Core.h"
#include "PhysicsLayers.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace Lynx
{
    struct RaycastHit
    {
        bool Hit = false;
        float Distance = 0.0f;
        glm::vec3 Point = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Normal = { 0.0f, 0.0f, 0.0f };
        Entity Entity;
    };
    
    class LX_API PhysicsSystem
    {
    public:
        PhysicsSystem(Scene* owner);
        ~PhysicsSystem();

        RaycastHit CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, JPH::BodyID ignoreBodyID = JPH::BodyID(0xFFFFFFFF));
        
        JPH::PhysicsSystem& GetJoltSystem() { return m_JoltPhysicsSystem; }
        JPH::BodyInterface& GetBodyInterface() { return m_JoltPhysicsSystem.GetBodyInterface(); }

    private:
        void Simulate(float deltaTime);
        
        // TODO:
        // We need to hold these implementations to keep Jolt happy
        // We will define these hidden classes in the .cpp file to keep the header clean,
        // so we use void* or unique_ptr pimpl style, or just forward declare them if we want type safety.
        // For simplicity in learning, we will declare pointers here and define classes in .cpp.
        class BPLayerInterfaceImpl* m_BPLayerInterface = nullptr;
        class ObjectVsBroadPhaseLayerFilterImpl* m_ObjectVsBroadPhaseLayerFilter = nullptr;
        class ObjectLayerPairFilterImpl* m_ObjectLayerPairFilter = nullptr;

        JPH::PhysicsSystem m_JoltPhysicsSystem;
        JPH::TempAllocatorImpl* m_TempAllocator = nullptr;
        JPH::JobSystemThreadPool* m_JobSystem = nullptr;
        
        Scene* m_OwnerScene = nullptr;
        
        friend class Scene;
    };
}
