#pragma once

#include "Lynx/Scene/Systems/ISystem.h"
#include "PhysicsTypes.h"
#include "Lynx/Asset/Asset.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Lynx
{
    class Scene;
    class PhysicsSystem : public ISystem
    {
    public:
        std::string GetName() const override { return "PhysicsSystem"; }
        
        void OnInit(Scene& scene) override;
        void OnSceneStart(Scene& scene) override;
        void OnShutdown(Scene& scene) override;
        void OnFixedUpdate(Scene& scene, float fixedDeltaTime) override;
        
    private:
        // Component lifecycle callbacks
        void OnRigidBodyAdded(entt::registry& registry, entt::entity entity);
        void OnRigidBodyRemoved(entt::registry& registry, entt::entity entity);
        void OnCharacterControllerAdded(entt::registry& registry, entt::entity entity);
        void OnCharacterControllerRemoved(entt::registry& registry, entt::entity entity);
        
        // Collider Callbacks
        void OnBoxColliderAdded(entt::registry& registry, entt::entity entity);
        void OnSphereColliderAdded(entt::registry& registry, entt::entity entity);
        void OnCapsuleColliderAdded(entt::registry& registry, entt::entity entity);
        void OnMeshColliderAdded(entt::registry& registry, entt::entity entity);
        
        // Internal Helpers
        void TryCreateRigidBody(Scene& scene, entt::entity entity);
        void CreatePendingRigidBodies(Scene& scene);
        void UpdateTerrainColliders(Scene& scene);
        void CreateCharacterController(Scene& scene, entt::entity entity);
        
        void UpdateCharacters(Scene& scene, float fixedDt);
        void SyncTransformsToPhysics(Scene& scene, float fixedDeltaTime);
        void SyncTransformsFromPhysics(Scene& scene);
        void ProcessCollisionEvents(Scene& scene);
        void SavePhysicsState(Scene& scene);
        
        // Entities waiting for colliders before creating rigid body
        std::unordered_set<entt::entity> m_PendingRigidBodies;
        
        // The bodies of a terrain with a collider, one per tile, and what they were made from: they are
        // made again when any of it changes. Kept here, a saved component holds nothing made at run time.
        struct TerrainBodies
        {
            AssetHandle Terrain = AssetHandle::Null();
            uint32_t Version = 0;
            glm::vec3 Origin{ 0.0f };
            float Friction = 0.0f;
            float Restitution = 0.0f;
            std::vector<BodyId> Bodies;
        };
        std::unordered_map<entt::entity, TerrainBodies> m_TerrainBodies;

        // Bodies/Characters to destroy at end of step
        std::vector<BodyId> m_BodiesToDestroy;
        std::vector<CharacterId> m_CharactersToDestroy;
        
        Scene* m_Scene = nullptr;
    };

}
