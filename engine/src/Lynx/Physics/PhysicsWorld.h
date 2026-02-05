#pragma once
#include "Lynx/Core.h"
#include "PhysicsTypes.h"

namespace Lynx
{
    // TODO: Need to add offsets to shapes?
    class LX_API PhysicsWorld
    {
    public:
        // --- Lifecycle ---
        PhysicsWorld();
        ~PhysicsWorld();
        
        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        
        void Step(float deltaTime);
        
        // --- Body Management ---
        BodyId CreateBody(const BodyDescriptor& desc);
        void DestroyBody(BodyId id);
        bool IsValidBody(BodyId id) const;
        
        // --- Getters ---
        glm::vec3 GetPosition(BodyId id) const;
        glm::quat GetRotation(BodyId id) const;
        glm::vec3 GetLinearVelocity(BodyId id) const;
        glm::vec3 GetAngularVelocity(BodyId id) const;
        glm::vec3 GetCenterOfMass(BodyId id) const;
        entt::entity GetEntity(BodyId id) const;
        bool IsActive(BodyId id) const;
        
        // --- Setters (for kinematic / direct manipulation) ---
        void SetPosition(BodyId id, const glm::vec3& position, bool activate = true);
        void SetRotation(BodyId id, const glm::quat& rotation, bool activate = true);
        void SetPositionAndRotation(BodyId id, const glm::vec3& position, const glm::quat& rotation, bool activate = true);
        void SetLinearVelocity(BodyId id, const glm::vec3& velocity);
        void SetAngularVelocity(BodyId id, const glm::vec3& velocity);
        void SetGravityFactor(BodyId id, float factor);
        
        void MoveKinematic(BodyId id, const glm::vec3& targetPos, const glm::quat& targetRot, float deltaTime);
        
        // --- Forces/Impulses (for dynamic bodies) ---
        void AddForce(BodyId id, const glm::vec3& force);
        void AddForceAtPoint(BodyId id, const glm::vec3& force, const glm::vec3& point);
        void AddImpulse(BodyId id, const glm::vec3& impulse);
        void AddImpulseAtPoint(BodyId id, const glm::vec3& impulse, const glm::vec3& point);
        void AddTorque(BodyId id, const glm::vec3& torque);
        void AddAngularImpulse(BodyId id, const glm::vec3& impulse);
        
        // --- Activation ---
        void Activate(BodyId id);
        void Deactivate(BodyId id);
        
        // --- Character Management ---
        CharacterId CreateCharacter(const CharacterDescriptor& desc);
        void DestroyCharacter(CharacterId id);
        bool IsValidCharacter(CharacterId id) const;
        
        // --- Character Getters ---
        glm::vec3 GetCharacterPosition(CharacterId id) const;
        glm::quat GetCharacterRotation(CharacterId id) const;
        glm::vec3 GetCharacterLinearVelocity(CharacterId id) const;
        glm::vec3 GetCharacterGroundVelocity(CharacterId id) const;
        glm::vec3 GetCharacterGroundNormal(CharacterId id) const;
        GroundState GetCharacterGroundState(CharacterId id) const;
        bool IsCharacterGrounded(CharacterId id) const;
        entt::entity GetCharacterEntity(CharacterId id) const;
        
        // --- Character Setters ---
        void SetCharacterPosition(CharacterId id, const glm::vec3& position);
        void SetCharacterRotation(CharacterId id, const glm::quat& rotation);
        void SetCharacterLinearVelocity(CharacterId id, const glm::vec3& velocity);
        
        // --- Character Update (call before step()) ---
        void UpdateCharacter(CharacterId id, float deltaTime, const glm::vec3& desiredVelocity);
        
        // --- Queries ---
        RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, uint8_t layerMask = 0xFF) const;
        std::vector<RaycastHit> RaycastAll(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, uint8_t layerMask = 0xFF) const;
        std::vector<BodyId> OverlapSphere(const glm::vec3& center, float radius, uint8_t layerMask = 0xFF) const;
        std::vector<BodyId> OverlapBox(const glm::vec3& center, const glm::vec3& halfExtent, const glm::quat& rotation, uint8_t layerMask = 0xFF) const;
        
        // --- Collision Events ---
        const std::vector<CollisionEvent>& GetCollisionEnterEvents() const;
        const std::vector<CollisionEvent>& GetCollisionStayEvents() const;
        const std::vector<CollisionEvent>& GetCollisionExitEvents() const;
        const std::vector<CharacterCollision>& GetCharacterCollisions() const;
        
        void SetGravity(const glm::vec3& gravity);
        glm::vec3 GetGravity() const;
        
    private:
        std::unique_ptr<class JoltPhysicsImpl> m_Impl;
    };
}
