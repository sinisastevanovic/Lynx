#pragma once

#include "Lynx/Core.h"
#include <entt/entt.hpp>
#include <glm/fwd.hpp>

#include "Lynx/Asset/Asset.h"
#include "Lynx/Event/Event.h"
#include "Systems/ParticleSystem.h"
#include "Systems/SystemManager.h"

namespace Lynx
{
    struct RigidBodyComponent;
    struct TransformComponent;
    class Prefab;
    class PhysicsSystem;
    class Entity;
    
    class LX_API Scene : public Asset, public std::enable_shared_from_this<Scene>
    {
    public:
        Scene(const std::string& filePath = "");
        ~Scene();
        
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        // --- Entity Management ---
        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity, bool excludeChildren = true);
        void DestroyEntity(entt::entity entity, bool excludeChildren = true);
        void DestroyEntityDeferred(Entity entity, bool excludeChildren = true);
        void DestroyEntityDeferred(entt::entity entity, bool excludeChildren = true);
        
        Entity InstantiatePrefab(std::shared_ptr<Prefab> prefab);
        Entity InstantiatePrefab(std::shared_ptr<Prefab> prefab, Entity parent);
        Entity InstantiatePrefab(AssetHandle prefab);
        Entity InstantiatePrefab(AssetHandle prefab, Entity parent);
        
        Entity FindEntityByName(const std::string& name);
        Entity FindEntityByUUID(UUID uuid);
        std::shared_ptr<class UIElement> FindUIElementByID(UUID id);
                
        void AttachEntity(entt::entity child, entt::entity parent);
        void AttachEntityKeepWorld(entt::entity child, entt::entity parent);
        void DetachEntity(entt::entity child);
        void DetachEntityKeepWorld(entt::entity child);
        
        // --- Iteration (thin wrapper, exposes entt but ergonomic) ---
        template <typename... Components>
        auto View()
        {
            return m_Registry.view<Components...>();
        }
        
        template <typename... Components>
        auto Group()
        {
            return m_Registry.group<Components...>();
        }
        
        entt::registry& Reg() { return m_Registry; }
        const entt::registry& Reg() const { return m_Registry; }
        
        // --- Events (scene-local) ---
        template <typename Event, auto Callback, typename Instance>
        void On(Instance* instance)
        {
            m_Dispatcher.sink<Event>().template connect<Callback>(instance);
        }
        
        template <typename Event, auto Callback, typename Instance>
        void Off(Instance* instance)
        {
            m_Dispatcher.sink<Event>().template disconnect<Callback>(instance);
        }
        
        template <typename Event, typename... Args>
        void Emit(Args&&... args)
        {
            m_Dispatcher.trigger(Event{std::forward<Args>(args)...});
        }
        
        template <typename Event, typename... Args>
        void Enqueue(Args&&... args)
        {
            m_Dispatcher.enqueue(Event{std::forward<Args>(args)...});
        }
        
        void DispatchEvents()
        {
            m_Dispatcher.update();
        }
        
        // --- Game Systems ---
        template <typename T, typename... Args>
        T* AddSystem(Args&&... args)
        {
            return m_GameSystems.AddSystem<T>(std::forward<Args>(args)...);
        }
        
        template <typename T>
        T* GetSystem()
        {
            return m_GameSystems.GetSystem<T>();
        }
        
        template <typename T>
        bool RemoveSystem()
        {
            return m_GameSystems.RemoveSystem<T>();
        }
        
        PhysicsSystem& GetPhysicsSystem() { return *m_PhysicsSystem; }
        ParticleSystem* GetParticleSystem() { return &m_ParticleSystem; }

        static AssetType GetAssetType() { return AssetType::Scene; }
        virtual AssetType GetType() const override { return Scene::GetAssetType(); }

    protected:
        virtual bool LoadSourceData() override;
        void PostDeserialize();
        
        
        void OnRuntimeStart();
        void OnRuntimeStop();
        
        void OnUpdateRuntime(float deltaTime);
        void OnFixedUpdate(float fixedDeltaTime);
        void OnLateUpdate(float deltaTime);
        void OnUpdateEditor(float deltaTime, glm::vec3 cameraPos);
        
        void OnEvent(Event& event);
        
        void UpdateGlobalTransforms();

    private:
        void UpdateEntityTransform(entt::entity entity, const glm::mat4& parentTransform);
        void CreateRuntimeBody(entt::entity entity, const TransformComponent& transform, RigidBodyComponent& rigidBody);
        void CreateRuntimeCharacter(entt::entity entity, const TransformComponent& transform, struct CharacterControllerComponent& character);
        void ProcessDestroyQueue();
        
    private:
        entt::registry m_Registry;
        entt::dispatcher m_Dispatcher;
        
        std::unique_ptr<PhysicsSystem> m_PhysicsSystem;
        ParticleSystem m_ParticleSystem;
        
        SystemManager m_GameSystems;
        
        std::vector<entt::entity> m_DestroyQueue;
        
        std::unordered_map<UUID, entt::entity> m_EntityMap; // TODO: Implement this!

        friend class SceneHierarchyPanel; // For editor later
        friend class SceneSerializer;
        friend class Engine;
        friend class EditorLayer;
    };
}
