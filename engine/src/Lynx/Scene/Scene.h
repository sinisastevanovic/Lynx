#pragma once

#include "Lynx/Core.h"
#include <entt/entt.hpp>
#include <glm/fwd.hpp>

#include "Lynx/Asset/Asset.h"
#include "Lynx/Event/Event.h"
#include "Systems/ParticleSystem.h"

namespace Lynx
{
    class Prefab;
    class PhysicsSystem;
    class Entity;

    class LX_API Scene : public Asset, public std::enable_shared_from_this<Scene>
    {
    public:
        Scene(const std::string& filePath = "");
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(entt::entity entity, bool excludeChildren = true);
        
        Entity InstantiatePrefab(std::shared_ptr<Prefab> prefab);
        Entity InstantiatePrefab(std::shared_ptr<Prefab> prefab, Entity parent);
        Entity InstantiatePrefab(AssetHandle prefab);
        Entity InstantiatePrefab(AssetHandle prefab, Entity parent);

        void OnRuntimeStart();
        void OnRuntimeStop();

        void OnUpdateRuntime(float deltaTime);
        void OnUpdateEditor(float deltaTime, glm::vec3 cameraPos);

        void OnEvent(Event& event);

        void AttachEntity(entt::entity child, entt::entity parent);
        void AttachEntityKeepWorld(entt::entity child, entt::entity parent);
        void DetachEntity(entt::entity child);
        void DetachEntityKeepWorld(entt::entity child);
        
        std::shared_ptr<class UIElement> FindUIElementByID(UUID id);

        void UpdateGlobalTransforms();
        
        PhysicsSystem& GetPhysicsSystem() { return *m_PhysicsSystem; }
        ParticleSystem* GetParticleSystem() { return &m_ParticleSystem; }

        static AssetType GetAssetType() { return AssetType::Scene; }
        virtual AssetType GetType() const override { return Scene::GetAssetType(); }

        // Temporary access to registry
        entt::registry& Reg() { return m_Registry; }

    protected:
        virtual bool LoadSourceData() override;
        
        void PostDeserialize();

    private:
        void UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform);
        
    private:
        entt::registry m_Registry;
        std::unique_ptr<PhysicsSystem> m_PhysicsSystem;
        ParticleSystem m_ParticleSystem;

        friend class Entity;
        friend class SceneHierarchyPanel; // For editor later
        friend class SceneSerializer;
    };
}
