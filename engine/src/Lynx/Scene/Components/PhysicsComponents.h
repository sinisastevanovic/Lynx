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
}