#include "SceneRenderer.h"

#include "DebugRenderer.h"
#include "EditorCamera.h"
#include "GpuScene.h"
#include "Renderer.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Profiling/FrameProfiler.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    namespace
    {
        // What the renderer keeps about a mesh entity. It lives in the scene's registry next to the
        // entity's own components, so it is found as fast as they are and goes away with the entity.
        // It is not a registered component: it is never saved, shown or copied into a played scene.
        struct MeshInstances
        {
            GpuScene::Range Range;

            // The mesh the instances were made from. Null for an entity whose mesh is not set or not loaded.
            const StaticMesh* Asset = nullptr;

            // The entity's place in the list of the mesh's users.
            size_t IndexAmongUsers = 0;
        };

        int EntityId(entt::entity entity)
        {
            return static_cast<int>(entity);
        }
    }

    SceneRenderer::SceneRenderer(Renderer& renderer, FrameProfiler& profiler, std::shared_ptr<Scene> scene)
        : m_Renderer(renderer), m_Profiler(profiler), m_Scene(std::move(scene))
    {
        if (m_Scene)
            Follow(*m_Scene);
    }

    SceneRenderer::~SceneRenderer()
    {
        if (m_Scene)
            Unfollow(*m_Scene);
    }

    void SceneRenderer::SetScene(std::shared_ptr<Scene> scene)
    {
        if (m_Scene)
            Unfollow(*m_Scene);

        m_Scene = std::move(scene);
        m_ViewportDirty = true;

        if (m_Scene)
            Follow(*m_Scene);
    }

    void SceneRenderer::Follow(Scene& scene)
    {
        entt::registry& registry = scene.Reg();

        // Left over if the scene was drawn before, by instances that are long gone.
        registry.clear<MeshInstances>();

        // Added, or given another mesh or other materials: both end in new instances.
        registry.on_construct<MeshComponent>().connect<&SceneRenderer::OnMeshAdded>(this);
        registry.on_update<MeshComponent>().connect<&SceneRenderer::OnMeshAdded>(this);
        registry.on_destroy<MeshComponent>().connect<&SceneRenderer::OnMeshRemoved>(this);
        registry.on_construct<DisabledComponent>().connect<&SceneRenderer::OnEntityDisabled>(this);
        registry.on_destroy<DisabledComponent>().connect<&SceneRenderer::OnEntityEnabled>(this);
        registry.on_destroy<MeshInstances>().connect<&SceneRenderer::OnInstancesRemoved>(this);

        for (const entt::entity entity : registry.view<MeshComponent>(entt::exclude<DisabledComponent>))
            m_PendingEntities.push_back(entity);

        // Everything in the scene is new to the renderer, so what moved before now does not matter.
        scene.ClearMovedEntities();
    }

    void SceneRenderer::Unfollow(Scene& scene)
    {
        entt::registry& registry = scene.Reg();
        registry.on_construct<MeshComponent>().disconnect(this);
        registry.on_update<MeshComponent>().disconnect(this);
        registry.on_destroy<MeshComponent>().disconnect(this);
        registry.on_construct<DisabledComponent>().disconnect(this);
        registry.on_destroy<DisabledComponent>().disconnect(this);
        registry.on_destroy<MeshInstances>().disconnect(this);

        m_Renderer.GetGpuScene().Clear();
        m_MeshUses.clear();
        m_PendingEntities.clear();
    }

    void SceneRenderer::OnMeshAdded(entt::registry&, entt::entity entity)
    {
        m_PendingEntities.push_back(entity);
    }

    void SceneRenderer::OnMeshRemoved(entt::registry& registry, entt::entity entity)
    {
        registry.remove<MeshInstances>(entity);
    }

    void SceneRenderer::OnEntityEnabled(entt::registry&, entt::entity entity)
    {
        m_PendingEntities.push_back(entity);
    }

    void SceneRenderer::OnEntityDisabled(entt::registry& registry, entt::entity entity)
    {
        registry.remove<MeshInstances>(entity);
    }

    void SceneRenderer::OnInstancesRemoved(entt::registry& registry, entt::entity entity)
    {
        const MeshInstances& instances = registry.get<MeshInstances>(entity);
        m_Renderer.GetGpuScene().Remove(instances.Range);

        const auto use = m_MeshUses.find(instances.Asset);
        if (use == m_MeshUses.end())
            return;

        // The last user takes the place of the one that leaves.
        std::vector<entt::entity>& users = use->second.Users;
        const entt::entity last = users.back();
        users[instances.IndexAmongUsers] = last;
        users.pop_back();
        if (last != entity)
            registry.get<MeshInstances>(last).IndexAmongUsers = instances.IndexAmongUsers;

        if (users.empty())
            m_MeshUses.erase(use);
    }

    void SceneRenderer::AddInstances(entt::entity entity, bool isEditor, float physicsAlpha)
    {
        entt::registry& registry = m_Scene->Reg();
        if (!registry.valid(entity) || registry.all_of<MeshInstances>(entity) || registry.all_of<DisabledComponent>(entity))
            return;

        const MeshComponent* meshComponent = registry.try_get<MeshComponent>(entity);
        const TransformComponent* transform = registry.try_get<TransformComponent>(entity);
        if (!meshComponent || !transform)
            return;

        // An entity without a mesh gets the component too, with nothing in it.
        MeshInstances instances;
        if (std::shared_ptr<StaticMesh> mesh = meshComponent->GetMesh().Get())
        {
            // A character moves in physics steps, which are fewer than frames, so it is drawn between its last two poses.
            const bool isInterpolated = !isEditor && registry.all_of<CharacterControllerComponent>(entity);
            const glm::mat4 matrix = isInterpolated ? transform->GetPhysicsInterpolatedTransform(physicsAlpha) : transform->GetWorldMatrix();

            m_GivenMaterials.clear();
            for (const AssetRef<Material>& material : meshComponent->GetMaterials())
                m_GivenMaterials.push_back(material.Get());

            instances.Asset = mesh.get();
            instances.Range = m_Renderer.GetGpuScene().Add(mesh, m_GivenMaterials, matrix, EntityId(entity));

            MeshUse& use = m_MeshUses[mesh.get()];
            if (!use.Mesh)
            {
                use.Mesh = mesh;
                use.Version = mesh->GetVersion();
            }
            instances.IndexAmongUsers = use.Users.size();
            use.Users.push_back(entity);
        }

        registry.emplace<MeshInstances>(entity, instances);
    }

    void SceneRenderer::SyncMeshes(bool isEditor, float physicsAlpha)
    {
        entt::registry& registry = m_Scene->Reg();
        GpuScene& gpuScene = m_Renderer.GetGpuScene();

        // A mesh that was loaded or reloaded since its instances were made: they are made anew.
        for (auto& [asset, use] : m_MeshUses)
        {
            if (use.Version == use.Mesh->GetVersion())
                continue;

            use.Version = use.Mesh->GetVersion();
            m_PendingEntities.insert(m_PendingEntities.end(), use.Users.begin(), use.Users.end());
        }

        // Pending entities get their instances, in place of the ones they had.
        std::vector<entt::entity> pending = std::move(m_PendingEntities);
        m_PendingEntities.clear();
        for (const entt::entity entity : pending)
        {
            if (registry.valid(entity))
                registry.remove<MeshInstances>(entity);
        }
        for (const entt::entity entity : pending)
            AddInstances(entity, isEditor, physicsAlpha);

        // The scene says which entities moved. Only their instances are written again.
        for (const entt::entity entity : m_Scene->GetMovedEntities())
        {
            if (!registry.valid(entity))
                continue;

            if (const MeshInstances* instances = registry.try_get<MeshInstances>(entity); instances && instances->Range.Count > 0)
                gpuScene.SetTransform(instances->Range, registry.get<TransformComponent>(entity).GetWorldMatrix());
        }
        m_Scene->ClearMovedEntities();

        if (isEditor)
            return;

        // A character is drawn between its last two physics poses, which is a new place in every frame.
        const auto characters = registry.view<TransformComponent, CharacterControllerComponent, MeshInstances>();
        for (const entt::entity entity : characters)
        {
            const MeshInstances& instances = characters.get<MeshInstances>(entity);
            if (instances.Range.Count > 0)
                gpuScene.SetTransform(instances.Range, characters.get<TransformComponent>(entity).GetPhysicsInterpolatedTransform(physicsAlpha));
        }
    }

    void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
    {
        if (m_ViewportWidth != width || m_ViewportHeight != height)
        {
            m_ViewportWidth = width;
            m_ViewportHeight = height;
            m_ViewportDirty = true;
        }
    }

    void SceneRenderer::RenderEditor(EditorCamera& camera, float deltaTime)
    {
        if (!m_Scene)
            return;

        if (m_ViewportDirty)
            camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
        m_ViewportDirty = false;

        Extract(camera.GetView(), camera.GetProjection(), camera.GetPosition(), deltaTime, true, 0.0f);
        m_Renderer.RenderScene(m_Extracted);
    }

    void SceneRenderer::RenderRuntime(float deltaTime, float physicsAlpha)
    {
        if (!m_Scene)
            return;

        auto& registry = m_Scene->Reg();
        const auto cameras = registry.view<TransformComponent, CameraComponent>();
        for (auto entity : cameras)
        {
            auto [transform, camera] = cameras.get<TransformComponent, CameraComponent>(entity);
            if (!camera.Primary)
                continue;

            if (!camera.FixedAspectRatio && m_ViewportDirty)
                camera.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
            m_ViewportDirty = false;

            // TODO: Take a look at this whole physics interpolated transform thing in the future...
            // The camera's pose between the last two physics steps, through every parent it is attached to.
            glm::mat4 worldMatrix = transform.GetPhysicsInterpolatedTransform(physicsAlpha);
            entt::entity current = entity;
            while (registry.all_of<RelationshipComponent>(current))
            {
                const entt::entity parent = registry.get<RelationshipComponent>(current).Parent;
                if (parent == entt::null)
                    break;

                current = parent;
                if (const TransformComponent* parentTransform = registry.try_get<TransformComponent>(current))
                    worldMatrix = parentTransform->GetPhysicsInterpolatedTransform(physicsAlpha) * worldMatrix;
            }

            Extract(glm::inverse(worldMatrix), camera.Camera.GetProjection(), transform.GetTranslation(), deltaTime, false, physicsAlpha);
            m_Renderer.RenderScene(m_Extracted);
            return;
        }
    }

    void SceneRenderer::Extract(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, float deltaTime, bool isEditor, float physicsAlpha)
    {
        ProfileScope extractScope(m_Profiler, "Extract");
        auto& registry = m_Scene->Reg();

        m_Extracted.Clear();
        m_Extracted.View = view;
        m_Extracted.Projection = projection;
        m_Extracted.CameraPosition = cameraPosition;
        m_Extracted.IsEditorView = isEditor;
        m_Extracted.Viewing = m_Viewing;
        m_Extracted.DeltaTime = deltaTime;

        // The first enabled sun lights the scene. Without one, the defaults of ExtractedScene apply.
        const ExtractedScene defaults;
        m_Extracted.SunDirection = defaults.SunDirection;
        m_Extracted.SunColor = defaults.SunColor;
        m_Extracted.SunIntensity = defaults.SunIntensity;

        const auto suns = registry.view<TransformComponent, DirectionalLightComponent>(entt::exclude<DisabledComponent>);
        for (auto entity : suns)
        {
            auto [transform, light] = suns.get<TransformComponent, DirectionalLightComponent>(entity);
            m_Extracted.SunDirection = glm::rotate(transform.GetRotation(), glm::vec3(0.0f, 0.0f, -1.0f));
            m_Extracted.SunColor = light.Color;
            m_Extracted.SunIntensity = light.Intensity;
            break;
        }

        SyncMeshes(isEditor, physicsAlpha);

        if (m_Renderer.GetShowColliders())
            DrawColliders();
        m_Extracted.DebugLines = DebugRenderer::GetLines();

        m_Scene->GetParticleSystem()->Extract(*m_Scene, cameraPosition, m_Extracted.Particles);

        for (auto entity : registry.view<UICanvasComponent>())
        {
            if (const std::shared_ptr<UICanvas>& canvas = registry.get<UICanvasComponent>(entity).Canvas)
                m_Extracted.Canvases.push_back(canvas);
        }
    }

    void SceneRenderer::DrawColliders()
    {
        auto& registry = m_Scene->Reg();
        const glm::vec4 color = { 0.0f, 1.0f, 0.0f, 1.0f };

        const auto boxes = registry.view<TransformComponent, BoxColliderComponent>();
        for (auto entity : boxes)
        {
            auto [transform, collider] = boxes.get<TransformComponent, BoxColliderComponent>(entity);
            const glm::vec3 position = transform.GetTranslation() + transform.GetRotation() * collider.Offset;
            const glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), position)
                * glm::toMat4(transform.GetRotation())
                * glm::scale(glm::mat4(1.0f), collider.HalfSize * 2.0f);

            DebugRenderer::DrawBox(colliderTransform, color);
        }

        const auto spheres = registry.view<TransformComponent, SphereColliderComponent>();
        for (auto entity : spheres)
        {
            auto [transform, collider] = spheres.get<TransformComponent, SphereColliderComponent>(entity);
            DebugRenderer::DrawSphere(transform.GetTranslation() + transform.GetRotation() * collider.Offset, collider.Radius, color);
        }

        const auto capsules = registry.view<TransformComponent, CapsuleColliderComponent>();
        for (auto entity : capsules)
        {
            auto [transform, collider] = capsules.get<TransformComponent, CapsuleColliderComponent>(entity);
            DebugRenderer::DrawCapsule(transform.GetTranslation() + transform.GetRotation() * collider.Offset, collider.Radius, collider.HalfHeight, transform.GetRotation(), color);
        }

        const auto characters = registry.view<TransformComponent, CharacterControllerComponent>();
        for (auto entity : characters)
        {
            auto [transform, character] = characters.get<TransformComponent, CharacterControllerComponent>(entity);
            DebugRenderer::DrawCapsule(transform.GetTranslation(), character.CapsuleRadius, character.CapsuleHalfHeight, transform.GetRotation(), color);
        }
    }
}
