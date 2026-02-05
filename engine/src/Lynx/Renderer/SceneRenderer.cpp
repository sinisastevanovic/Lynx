#include "SceneRenderer.h"

#include "DebugRenderer.h"
#include "Lynx/Engine.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

namespace Lynx
{
    SceneRenderer::SceneRenderer(std::shared_ptr<Scene> scene)
        : m_Scene(scene)
    {
    }

    void SceneRenderer::SetScene(std::shared_ptr<Scene> scene)
    {
        m_Scene = scene;
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
        SubmitScene(camera.GetView(), camera.GetProjection(), camera.GetPosition(), deltaTime, true);
        m_ViewportDirty = false;
    }

    void SceneRenderer::RenderRuntime(float deltaTime, float physicsAlpha)
    {
        if (!m_Scene)
            return;
        
        {
            auto view = m_Scene->Reg().view<TransformComponent, CameraComponent>();
            for (auto entity : view)
            {
                auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
                if (camera.Primary)
                {
                    if (!camera.FixedAspectRatio && m_ViewportDirty)
                        camera.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
                    
                    // TODO: Take a look at this whole physics interpolated transform thing in the future...
                    // I don't really know if this is the correct way to do that. It works for now, but I don't know.
                    // Calculate Interpolated World Matrix for Camera
                    // This handles both attached (child) and detached (scripted) cameras correctly
                    // by walking the hierarchy and interpolating each node.
                    glm::mat4 interpolatedWorldMatrix = transform.GetPhysicsInterpolatedTransform(physicsAlpha);
                    
                    auto& reg = m_Scene->Reg();
                    entt::entity current = entity;
                    
                    while(reg.all_of<RelationshipComponent>(current))
                    {
                        auto& rel = reg.get<RelationshipComponent>(current);
                        if (rel.Parent == entt::null)
                            break;
                            
                        current = rel.Parent;
                        if (reg.all_of<TransformComponent>(current))
                        {
                            auto& parentTransform = reg.get<TransformComponent>(current);
                            interpolatedWorldMatrix = parentTransform.GetPhysicsInterpolatedTransform(physicsAlpha) * interpolatedWorldMatrix;
                        }
                    }

                    SubmitScene(glm::inverse(interpolatedWorldMatrix), camera.Camera.GetProjection(), transform.Translation, deltaTime, false, physicsAlpha);
                    m_ViewportDirty = false;
                    break;
                }
            }
        }
    }

    void SceneRenderer::SubmitScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos, float deltaTime, bool isEditor, float physicsAlpha)
    {
        auto& renderer = Engine::Get().GetRenderer();
        auto& assetManager = Engine::Get().GetAssetManager();
        
        glm::vec3 lightDir = { -0.5f, -0.7f, 1.0f };
        glm::vec3 lightColor = { 1.0f, 1.0f, 1.0f };
        float lightIntensity = 1.0f;

        auto sunView = m_Scene->Reg().view<TransformComponent, DirectionalLightComponent>(entt::exclude<DisabledComponent>);
        for (auto entity : sunView)
        {
            auto [transform, light] = sunView.get<TransformComponent, DirectionalLightComponent>(entity);
            lightDir = glm::rotate(transform.Rotation, glm::vec3(0.0f, 0.0f, -1.0f));
            lightColor = light.Color;
            lightIntensity = light.Intensity;
            break;
        }
        
        renderer.BeginScene(view, projection, cameraPos, lightDir, lightColor, lightIntensity, deltaTime, isEditor);
        
        Frustum camFrustum;
        camFrustum.FromViewProjection(projection * view);
        Frustum lightFrustum;
        lightFrustum.FromViewProjection(renderer.GetLightViewProjMatrix());
        
        if (!isEditor)
        {
            auto physicsView = m_Scene->Reg().view<TransformComponent, MeshComponent, CharacterControllerComponent>(entt::exclude<DisabledComponent>);
            for (auto entity : physicsView)
            {
                auto [transform, mesh] = physicsView.get<TransformComponent, MeshComponent>(entity);
                if (mesh.Mesh)
                {
                    auto finalTransform = transform.GetPhysicsInterpolatedTransform(physicsAlpha);
                    AABB worldBounds = TransformAABB(mesh.Mesh->GetBounds(), finalTransform);
                    // TODO: Check if this is worth it. Using these flags splits the batches up, so more draw calls, but less geometry drawn...
                    RenderFlags flags = RenderFlags::None;
                    if (camFrustum.IsOnFrustum(worldBounds))
                        flags = flags | RenderFlags::MainPass;
                    if (lightFrustum.IsOnFrustum(worldBounds))
                        flags = flags | RenderFlags::ShadowPass;
                    if (flags != RenderFlags::None)
                    {
                        renderer.SubmitMesh(mesh.Mesh.Get(), finalTransform, flags, (int)entity);
                    }
                }
            }
            
            auto meshView = m_Scene->Reg().view<TransformComponent, MeshComponent>(entt::exclude<DisabledComponent, CharacterControllerComponent>);
            for (auto entity : meshView)
            {
                auto [transform, meshComp] = meshView.get<TransformComponent, MeshComponent>(entity);
            
                if (meshComp.Mesh)
                {
                    AABB worldBounds = TransformAABB(meshComp.Mesh->GetBounds(), transform.WorldMatrix);
                    // TODO: Check if this is worth it. Using these flags splits the batches up, so more draw calls, but less geometry drawn...
                    RenderFlags flags = RenderFlags::None;
                    if (camFrustum.IsOnFrustum(worldBounds))
                        flags = flags | RenderFlags::MainPass;
                    if (lightFrustum.IsOnFrustum(worldBounds))
                        flags = flags | RenderFlags::ShadowPass;
                    if (flags != RenderFlags::None)
                    {
                        renderer.SubmitMesh(meshComp.Mesh.Get(), transform.WorldMatrix, flags, (int)entity);
                    }
                }
            }
        }
        else
        {
            auto meshView = m_Scene->Reg().view<TransformComponent, MeshComponent>(entt::exclude<DisabledComponent>);
            for (auto entity : meshView)
            {
                auto [transform, meshComp] = meshView.get<TransformComponent, MeshComponent>(entity);
            
                if (meshComp.Mesh)
                {
                    AABB worldBounds = TransformAABB(meshComp.Mesh->GetBounds(), transform.WorldMatrix);
                    // TODO: Check if this is worth it. Using these flags splits the batches up, so more draw calls, but less geometry drawn...
                    RenderFlags flags = RenderFlags::None;
                    if (camFrustum.IsOnFrustum(worldBounds))
                        flags = flags | RenderFlags::MainPass;
                    if (lightFrustum.IsOnFrustum(worldBounds))
                        flags = flags | RenderFlags::ShadowPass;
                    if (flags != RenderFlags::None)
                    {
                        renderer.SubmitMesh(meshComp.Mesh.Get(), transform.WorldMatrix, flags, (int)entity);
                    }
                }
            }
        }
        
        
        
        
        if (renderer.GetShowColliders()) // Render collider meshes
        {
            glm::vec4 debugColor = { 0.0f, 1.0f, 0.0f, 1.0f };
                
            auto boxView = m_Scene->Reg().view<TransformComponent, BoxColliderComponent>();
            for (auto entity : boxView)
            {
                auto [transform, collider] = boxView.get<TransformComponent, BoxColliderComponent>(entity);
                
                glm::vec3 worldOffset = transform.Rotation * collider.Offset;
                glm::vec3 worldPos = transform.Translation + worldOffset;
                
                glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), worldPos)
                    * glm::toMat4(transform.Rotation)
                    * glm::scale(glm::mat4(1.0f), collider.HalfSize * 2.0f);

                DebugRenderer::DrawBox(colliderTransform, debugColor);
            }

            auto sphereView = m_Scene->Reg().view<TransformComponent, SphereColliderComponent>();
            for (auto entity : sphereView)
            {
                auto [transform, collider] = sphereView.get<TransformComponent, SphereColliderComponent>(entity);
                glm::vec3 worldOffset = transform.Rotation * collider.Offset;
                DebugRenderer::DrawSphere(transform.Translation + worldOffset, collider.Radius, debugColor);
            }

            auto capsuleView = m_Scene->Reg().view<TransformComponent, CapsuleColliderComponent>();
            for (auto entity : capsuleView)
            {
                auto [transform, collider] = capsuleView.get<TransformComponent, CapsuleColliderComponent>(entity);
                glm::vec3 worldOffset = transform.Rotation * collider.Offset;
                DebugRenderer::DrawCapsule(transform.Translation + worldOffset, collider.Radius, collider.HalfHeight, transform.Rotation, debugColor);
            }
            
            auto charView = m_Scene->Reg().view<TransformComponent, CharacterControllerComponent>();
            for (auto entity : charView)
            {
                auto [transform, character] = charView.get<TransformComponent, CharacterControllerComponent>(entity);
                //glm::vec3 worldOffset = transform.Rotation * character.Offset;
                DebugRenderer::DrawCapsule(transform.Translation, character.CapsuleRadius, character.CapsuleHalfHeight, transform.Rotation, debugColor);
            }
        }
        
        m_Scene->GetParticleSystem()->OnUpdate(deltaTime, m_Scene.get(), cameraPos);

        renderer.EndScene();
    }
}
