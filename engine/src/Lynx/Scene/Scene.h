#pragma once

#include "Lynx/Core.h"
#include <entt/entt.hpp>
#include <glm/fwd.hpp>

#include "Lynx/Asset/Asset.h"

namespace Lynx
{
    class Entity;

    class LX_API Scene : public Asset, public std::enable_shared_from_this<Scene>
    {
    public:
        Scene(const std::string& filePath = "");
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(entt::entity entity, bool excludeChildren = true);

        void OnRuntimeStart();
        void OnRuntimeStop();

        void OnUpdateRuntime(float deltaTime);
        void OnUpdateEditor(float deltaTime);

        void AttachEntity(entt::entity child, entt::entity parent);
        void AttachEntityKeepWorld(entt::entity child, entt::entity parent);
        void DetachEntity(entt::entity child);
        void DetachEntityKeepWorld(entt::entity child);

        void UpdateGlobalTransforms();

        static AssetType GetAssetType() { return AssetType::Scene; }
        virtual AssetType GetType() const override { return Scene::GetAssetType(); }

        // Temporary access to registry
        entt::registry& Reg() { return m_Registry; }

    protected:
        virtual bool LoadSourceData() override;

    private:
        
        void UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform);
        
        entt::registry m_Registry;

        friend class Entity;
        friend class SceneHierarchyPanel; // For editor later
    };
}
