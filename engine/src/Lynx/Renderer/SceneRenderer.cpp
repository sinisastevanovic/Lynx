#include "SceneRenderer.h"

#include "DebugRenderer.h"
#include "EditorCamera.h"
#include "Renderer.h"
#include "Lynx/Profiling/FrameProfiler.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    SceneRenderer::SceneRenderer(Renderer& renderer, FrameProfiler& profiler, std::shared_ptr<Scene> scene)
        : m_Renderer(renderer), m_Profiler(profiler), m_Scene(std::move(scene))
    {
    }

    void SceneRenderer::SetScene(std::shared_ptr<Scene> scene)
    {
        m_Scene = std::move(scene);
        m_ViewportDirty = true;
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

            Extract(glm::inverse(worldMatrix), camera.Camera.GetProjection(), transform.Translation, deltaTime, false, physicsAlpha);
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
            m_Extracted.SunDirection = glm::rotate(transform.Rotation, glm::vec3(0.0f, 0.0f, -1.0f));
            m_Extracted.SunColor = light.Color;
            m_Extracted.SunIntensity = light.Intensity;
            break;
        }

        ExtractMeshes(isEditor, physicsAlpha);

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

    void SceneRenderer::ExtractMeshes(bool isEditor, float physicsAlpha)
    {
        auto& registry = m_Scene->Reg();

        const auto addMesh = [this](entt::entity entity, const MeshComponent& mesh, const glm::mat4& transform)
        {
            if (std::shared_ptr<StaticMesh> asset = mesh.Mesh.Get())
                m_Extracted.Meshes.push_back({ std::move(asset), transform, static_cast<int>(entity) });
        };

        if (isEditor)
        {
            const auto meshes = registry.view<TransformComponent, MeshComponent>(entt::exclude<DisabledComponent>);
            for (auto entity : meshes)
            {
                auto [transform, mesh] = meshes.get<TransformComponent, MeshComponent>(entity);
                addMesh(entity, mesh, transform.WorldMatrix);
            }
            return;
        }

        // A character moves in physics steps, which are fewer than frames, so it is drawn between its last two poses.
        const auto characters = registry.view<TransformComponent, MeshComponent, CharacterControllerComponent>(entt::exclude<DisabledComponent>);
        for (auto entity : characters)
        {
            auto [transform, mesh] = characters.get<TransformComponent, MeshComponent>(entity);
            addMesh(entity, mesh, transform.GetPhysicsInterpolatedTransform(physicsAlpha));
        }

        const auto meshes = registry.view<TransformComponent, MeshComponent>(entt::exclude<DisabledComponent, CharacterControllerComponent>);
        for (auto entity : meshes)
        {
            auto [transform, mesh] = meshes.get<TransformComponent, MeshComponent>(entity);
            addMesh(entity, mesh, transform.WorldMatrix);
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
            const glm::vec3 position = transform.Translation + transform.Rotation * collider.Offset;
            const glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), position)
                * glm::toMat4(transform.Rotation)
                * glm::scale(glm::mat4(1.0f), collider.HalfSize * 2.0f);

            DebugRenderer::DrawBox(colliderTransform, color);
        }

        const auto spheres = registry.view<TransformComponent, SphereColliderComponent>();
        for (auto entity : spheres)
        {
            auto [transform, collider] = spheres.get<TransformComponent, SphereColliderComponent>(entity);
            DebugRenderer::DrawSphere(transform.Translation + transform.Rotation * collider.Offset, collider.Radius, color);
        }

        const auto capsules = registry.view<TransformComponent, CapsuleColliderComponent>();
        for (auto entity : capsules)
        {
            auto [transform, collider] = capsules.get<TransformComponent, CapsuleColliderComponent>(entity);
            DebugRenderer::DrawCapsule(transform.Translation + transform.Rotation * collider.Offset, collider.Radius, collider.HalfHeight, transform.Rotation, color);
        }

        const auto characters = registry.view<TransformComponent, CharacterControllerComponent>();
        for (auto entity : characters)
        {
            auto [transform, character] = characters.get<TransformComponent, CharacterControllerComponent>(entity);
            DebugRenderer::DrawCapsule(transform.Translation, character.CapsuleRadius, character.CapsuleHalfHeight, transform.Rotation, color);
        }
    }
}
