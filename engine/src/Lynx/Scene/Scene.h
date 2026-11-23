#pragma once

#include "Lynx/Core.h"
#include <entt/entt.hpp>
#include <glm/fwd.hpp>

#include <span>

#include "Entity.h"
#include "Lynx/Asset/Asset.h"
#include "Lynx/Event/Event.h"
#include "Lynx/Physics/PhysicsSystem.h"
#include "Systems/ParticleSystem.h"
#include "Systems/SystemManager.h"

namespace Lynx
{
    class PhysicsWorld;
    struct RigidBodyComponent;
    struct TransformComponent;
    class Prefab;
    
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
        
        template <typename... Components, typename Func>
        void ForEach(Func&& func)
        {
            auto view = m_Registry.view<Components...>();
            
            view.each([&](auto entityID, auto&... components)
            {
                Entity entityWrapper(entityID, this);
                func(entityWrapper, components...);
            });
        }
        
        entt::registry& Reg() { return m_Registry; }
        const entt::registry& Reg() const { return m_Registry; }

        // --- Change tracking ---
        // The entities whose place in the world has changed since the list was last cleared. An entity can be
        // in it more than once. Whoever keeps something in step with the scene, as the renderer does with what
        // it has on the GPU, reads the list and clears it, and so only looks at what moved.
        std::span<const entt::entity> GetMovedEntities() const { return m_MovedEntities; }
        void ClearMovedEntities() { m_MovedEntities.clear(); }
        
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
        
        PhysicsWorld* GetPhysicsWorld() { return m_PhysicsWorld.get(); }
        PhysicsWorld& GetPhysicsWorldChecked() { LX_ASSERT(m_PhysicsWorld, "PhysicsWorld only available in player mode!"); return *m_PhysicsWorld; }
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
        
        // Works out the world matrix of every entity whose transform was written to since the last call,
        // and of its children. Entities that were not written to are not looked at.
        void UpdateGlobalTransforms();

    public:
        // The scene a registry belongs to, for code that is handed only the registry.
        static Scene& Of(entt::registry& registry);

        // The only ways to change where an entity is and what it is drawn with. The components hand
        // their values out for reading only, so that no change can go unnoticed: the scene puts the
        // entity on a list here, and whoever depends on the value is told.
        // Position, rotation and scale are relative to the entity's parent.
        void SetTranslation(entt::entity entity, const glm::vec3& translation);
        void SetRotation(entt::entity entity, const glm::quat& rotation);
        void SetScale(entt::entity entity, const glm::vec3& scale);
        void SetLocalTransform(entt::entity entity, const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);

        void SetMesh(entt::entity entity, AssetHandle mesh);

        // The material that takes the place of the mesh's own for one submesh. A null handle gives
        // the submesh its own material back.
        void SetMaterial(entt::entity entity, size_t submeshIndex, AssetHandle material);

    private:
        void MarkTransformChanged(entt::entity entity, TransformComponent& transform);
        void OnTransformWritten(entt::registry& registry, entt::entity entity);
        void UpdateWorldMatrices(entt::entity entity, TransformComponent& transform, const glm::mat4* parentWorld);
        bool SetWorldMatrix(entt::entity entity, TransformComponent& transform, const glm::mat4& worldMatrix);
        void SetLocalFromMatrix(entt::entity entity, const glm::mat4& matrix);
        void ProcessDestroyQueue();
        
    private:
        entt::registry m_Registry;
        entt::dispatcher m_Dispatcher;
        
        ParticleSystem m_ParticleSystem;
        std::unique_ptr<PhysicsWorld> m_PhysicsWorld;
        PhysicsSystem m_PhysicsSystem;
        
        SystemManager m_GameSystems;
        
        struct PendingDestroy
        {
            entt::entity Entity;
            bool ExcludeChildren;
        };
        std::vector<PendingDestroy> m_DestroyQueue;
        
        std::unordered_map<UUID, entt::entity> m_EntityMap;
        std::vector<entt::entity> m_MovedEntities;

        // Entities whose transform was written to since the last update of transforms, and the list
        // that update works through, kept for its memory.
        std::vector<entt::entity> m_ChangedTransforms;
        std::vector<entt::entity> m_ChangedTransformsInWork;

        friend class SceneHierarchyPanel; // For editor later
        friend class SceneSerializer;
        friend class Engine;
        friend class EditorLayer;
    };
}
