#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <glm/glm.hpp>

namespace Lynx
{
    enum class RigidBodyType { Static, Dynamic, Kinematic };

    struct RigidBodyComponent
    {
        RigidBodyType Type = RigidBodyType::Static;
        JPH::BodyID BodyId;
        bool RuntimeBodyCreated = false;

        bool LockRotationX = false;
        bool LockRotationY = false;
        bool LockRotationZ = false;

        void LockAllRotation() { LockRotationX = true; LockRotationY = true; LockRotationZ = true; }

        RigidBodyComponent() = default;
        RigidBodyComponent(const RigidBodyComponent&) = default;
        RigidBodyComponent(RigidBodyType type) : Type(type) {}
    };

    struct BoxColliderComponent
    {
        glm::vec3 HalfSize = { 0.5f, 0.5f, 0.5f };

        BoxColliderComponent() = default;
        BoxColliderComponent(const BoxColliderComponent&) = default;
    };

    struct SphereColliderComponent
    {
        float Radius = 0.5f;

        SphereColliderComponent() = default;
        SphereColliderComponent(const SphereColliderComponent&) = default;
    };

    struct CapsuleColliderComponent
    {
        float Radius = 0.5f;
        float Height = 1.0f;

        CapsuleColliderComponent() = default;
        CapsuleColliderComponent(const CapsuleColliderComponent&) = default;
    };
}