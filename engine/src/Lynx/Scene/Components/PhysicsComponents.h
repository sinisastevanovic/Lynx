#pragma once

#include "Lynx/Physics/PhysicsTypes.h"
#include <glm/glm.hpp>

namespace JPH
{
    class CharacterVirtual;
}

namespace Lynx
{
    struct RigidBodyComponent
    {
        BodyType Type = BodyType::Static;
        float Mass = 1.0f;
        float Friction = 0.5f;
        float Restitution = 0.0f;
        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;
        float GravityFactor = 1.0f;
        MotionQuality MotionQuality = MotionQuality::Discrete;
        uint8_t Layer = CollisionLayer::Static;
        
        BodyId BodyId = INVALID_BODY;

        // TODO: Implement these!
        bool LockRotationX = false;
        bool LockRotationY = false;
        bool LockRotationZ = false;

        void LockAllRotation() { LockRotationX = true; LockRotationY = true; LockRotationZ = true; }

        RigidBodyComponent() = default;
        RigidBodyComponent(const RigidBodyComponent&) = default;
        RigidBodyComponent(BodyType type) : Type(type) {}
    };

    struct BoxColliderComponent
    {
        glm::vec3 HalfSize = { 0.5f, 0.5f, 0.5f };
        glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
        bool IsTrigger = false;

        BoxColliderComponent() = default;
        BoxColliderComponent(const BoxColliderComponent&) = default;
    };

    struct SphereColliderComponent
    {
        float Radius = 0.5f;
        glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
        bool IsTrigger = false;

        SphereColliderComponent() = default;
        SphereColliderComponent(const SphereColliderComponent&) = default;
    };

    struct CapsuleColliderComponent
    {
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
        glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
        bool IsTrigger = false;

        CapsuleColliderComponent() = default;
        CapsuleColliderComponent(const CapsuleColliderComponent&) = default;
    };
    
    struct MeshColliderComponent
    {
        AssetRef<StaticMesh> Mesh;
        bool IsTrigger = false;
    };
    
    struct CharacterControllerComponent
    {
        // Shape Settings
        float CapsuleRadius = 0.4f;
        float CapsuleHalfHeight = 0.9f;
        
        // Movement Settings
        float MaxSlopeAngle = 45.0f;
        float Mass = 80.0f;
        float MaxStrength = 100.0f;
        float CharacterPadding = 0.02f;
        float StepHeight = 0.4f; // TODO: ??
        float Gravity = -20.0f;
        float AirControlFactor = 0.3f;
        uint8_t Layer = CollisionLayer::Character;
        
        // Input
        glm::vec3 DesiredVelocity{0.0f};
        
        // State
        glm::vec3 Velocity{0.0f};
        GroundState GroundState = GroundState::InAir;
        glm::vec3 GroundNormal{0.0f, 1.0f, 0.0f};
        glm::vec3 GroundVelocity{0.0f}; // Velocity of ground (moving platforms)
        
        CharacterId CharacterId = INVALID_CHARACTER;
        
        CharacterControllerComponent() = default;
        CharacterControllerComponent(const CharacterControllerComponent&) = default;
    };
}