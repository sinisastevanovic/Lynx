#include "Viewport.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "../EditorLayer.h"

#include "Lynx/ImGui/ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>

#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Components/Components.h"

namespace Lynx
{
    void Viewport::SetOwner(EditorLayer* owner)
    {
        m_Owner = owner;
    }

    void Viewport::OnImGuiRender()
    {
        auto& renderer = Lynx::Engine::Get().GetRenderer();
        ImGui::Begin("Viewport");

        bool isFocused = ImGui::IsWindowFocused();
        bool isHovered = ImGui::IsWindowHovered();

        Engine::Get().SetBlockEvents(!isFocused);

        if (isFocused && Engine().Get().GetSceneState() != SceneState::Play)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_CurrentGizmoOperation = -1;
            if (ImGui::IsKeyPressed(ImGuiKey_W)) m_CurrentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_CurrentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_CurrentGizmoOperation = ImGuizmo::SCALE;
        }

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        if (viewportSize.x > 0 && viewportSize.y > 0)
            renderer.EnsureEditorViewport((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
        
        auto texture = renderer.GetViewportTexture();
        if (texture)
        {
            ImGui::Image((ImTextureID)texture.Get(), viewportSize);
        }

        auto selectedEntity = m_Owner->GetSelectedEntity();
        if (selectedEntity != entt::null && m_CurrentGizmoOperation != -1 && Engine().Get().GetSceneState() != SceneState::Play)
        {
            ImGuizmo::BeginFrame();
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            float windowWidth = (float)ImGui::GetWindowWidth();
            float windowHeight = (float)ImGui::GetWindowHeight();
            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);

            auto& camera = Engine::Get().GetEditorCamera();
            const glm::mat4& cameraView = camera.GetViewMatrix();
            const glm::mat4& cameraProjection = camera.GetProjection();

            auto scene = Engine::Get().GetActiveScene();
            auto& tc = scene->Reg().get<TransformComponent>(selectedEntity);
            glm::mat4 transform = tc.GetTransform();

            bool snap = ImGui::GetIO().KeyCtrl;
            float snapValue = 0.5f;
            if (m_CurrentGizmoOperation == ImGuizmo::ROTATE)
                snapValue = 45.0f;

            float snapValues[3] = { snapValue, snapValue, snapValue };

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_CurrentGizmoOperation,
                ImGuizmo::MODE::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

            if (ImGuizmo::IsUsing())
            {
                float matrixTranslation[3], matrixRotation[3], matrixScale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), matrixTranslation, matrixRotation, matrixScale);

                tc.Translation = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
                tc.SetRotationEuler(glm::radians(glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2])));
                tc.Scale = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
            }
        }

        auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
        auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        auto viewportOffset = ImGui::GetWindowPos();

        glm::vec2 bounds[2];
        bounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
        bounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

        if (isFocused && !ImGuizmo::IsUsing())
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                auto mousePos = ImGui::GetMousePos();
                mousePos.x -= bounds[0].x;
                mousePos.y -= bounds[0].y;

                glm::vec2 viewportSize2 = { bounds[1].x - bounds[0].x, bounds[1].y - bounds[0].y };

                int mouseX = (int)mousePos.x;
                int mouseY = (int)mousePos.y;

                if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize2.x && mouseY < (int)viewportSize2.y)
                {
                    int selectedID = renderer.ReadIdFromBuffer(mouseX, mouseY);
                    if (selectedID >= 0)
                    {
                        m_Owner->SetSelectedEntity((entt::entity)selectedID);
                    }
                    else
                    {
                        m_Owner->SetSelectedEntity(entt::null);
                    }
                }
            }
        }

        if (Engine::Get().GetSceneState() != SceneState::Play && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetUtils::GetDragDropPayload(AssetType::StaticMesh)))
            {
                uint64_t data = *(const uint64_t*)payload->Data;

                Scene* currScene = Engine::Get().GetActiveScene().get();
                auto newEntity = currScene->CreateEntity();
                auto& meshComp = newEntity.AddComponent<MeshComponent>();
                meshComp.Mesh = AssetHandle(data);
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::End();
    }
}
