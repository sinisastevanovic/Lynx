#pragma once
#include <variant>
#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

namespace Lynx
{
    using BodyId = uint32_t; // Opaque handle, maps to JPH::BodyID internally
    using CharacterId = uint32_t;
    constexpr BodyId INVALID_BODY = ~0u;
    constexpr CharacterId INVALID_CHARACTER = ~0u;

    enum class BodyType : uint8_t
    {
        Static,     // Never moves (walls, floors)
        Dynamic,    // Simulated by physics
        Kinematic   // Moved by code, affects dynamic bodies
    };

    enum class MotionQuality : uint8_t
    {
        Discrete,   // Fast, can tunnel at high speeds
        Continuous  // Slower, prevents tunneling
    };
    
    enum class GroundState : uint8_t
    {
        Grounded,
        OnSteepGround,
        NotSupported,
        InAir
    };

    namespace CollisionLayer 
    {
        constexpr uint8_t Static = 0;
        constexpr uint8_t Dynamic = 1;
        constexpr uint8_t Player = 2;
        constexpr uint8_t Character = 3;
        constexpr uint8_t Trigger = 4;
        constexpr uint8_t COUNT = 5;
    }
    
    struct BoxShape
    {
        glm::vec3 HalfExtents{0.5f};
    };
    
    struct SphereShape
    {
        float Radius = 0.5f;
    };
    
    struct CapsuleShape
    {
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
    };
    
    struct CylinderShape
    {
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
    };
    
    struct MeshShape
    {
        std::vector<glm::vec3> Vertices;
        std::vector<uint32_t> Indices;
    };
    
    using CollisionShape = std::variant<BoxShape, SphereShape, CapsuleShape, CylinderShape, MeshShape>;
    
    struct BodyDescriptor
    {
        CollisionShape Shape;
        glm::vec3 Position{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        
        BodyType Type = BodyType::Dynamic;
        MotionQuality MotionQuality = MotionQuality::Discrete;
        uint8_t Layer = CollisionLayer::Dynamic;
        
        float Mass = 1.0f; // Ignored for static/kinematic
        float Friction = 0.5f;
        float Restitution = 0.0f; // Bounciness
        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;
        float GravityFactor = 1.0f;
        
        bool IsTrigger = false; // No physical response, just events
        entt::entity Entity = entt::null;
    };
    
    struct CharacterDescriptor
    {
        glm::vec3 Position{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        
        float CapsuleRadius = 0.4f;
        float CapsuleHalfHeight = 0.9f;
        
        float Mass = 80.0f;
        float MaxSlopeAngle = glm::radians(50.0f);
        float MaxStrength = 100.0f;
        float PredicitveContactDistance = 0.1f;
        float PenetrationRecoverySpeed = 1.0f;
        
        uint8_t Layer = CollisionLayer::Character;
        entt::entity Entity = entt::null;
    };
    
    struct CollisionEvent
    {
        BodyId BodyA = INVALID_BODY;
        BodyId BodyB = INVALID_BODY;
        entt::entity EntityA = entt::null;
        entt::entity EntityB = entt::null;
        glm::vec3 ContactPoint{0.0f};
        glm::vec3 ContactNormal{0.0f};
        float PenetrationDepth = 0.0f;
        bool IsTrigger = false;
    };
    
    struct CharacterCollision
    {
        CharacterId CharacterA = INVALID_CHARACTER;
        BodyId BodyA = INVALID_BODY;
        CharacterId OtherCharacterId = INVALID_CHARACTER;
        entt::entity CharacterEntity = entt::null;
        entt::entity OtherEntity = entt::null;
        glm::vec3 ContactPoint{0.0f};
        glm::vec3 ContactNormal{0.0f};
    };
    
    struct RaycastHit
    {
        bool Hit = false;
        BodyId BodyId = INVALID_BODY;
        entt::entity Entity = entt::null;
        glm::vec3 Point{0.0f};
        glm::vec3 Normal{0.0f};
        float Distance = 0.0f;
    };
}
