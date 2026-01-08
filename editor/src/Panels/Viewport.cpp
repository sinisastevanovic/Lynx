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
    void Viewport::OnImGuiRender()
    {
        auto& renderer = Lynx::Engine::Get().GetRenderer();
        ImGui::Begin("Viewport");

        m_IsFocused = ImGui::IsWindowFocused();
        m_IsHovered = ImGui::IsWindowHovered();

        Engine::Get().SetBlockEvents(!m_IsFocused);
        Engine::Get().GetEditorCamera().SetViewportFocused(m_IsHovered && m_IsFocused);

        if (m_IsFocused && Engine().Get().GetSceneState() != SceneState::Play)
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

        // UI GIZMOS
        if (m_SelectedUIElement && Engine::Get().GetRenderer().GetShowUI())
        {
            ImVec2 viewStart = ImGui::GetItemRectMin();
            ImVec2 viewSize = ImGui::GetItemRectSize();

            if (viewportSize.x > 0 && viewportSize.y > 0)
            {
                float uiScale = 1.0f;
                if (auto canvas = std::dynamic_pointer_cast<UICanvas>(m_SelectedUIElement)) {
                    uiScale = canvas->GetScaleFactor();
                }
                else {
                    // Walk up tree to find canvas
                    auto parent = m_SelectedUIElement->GetParent();
                    while (parent) {
                        if (auto c = std::dynamic_pointer_cast<UICanvas>(parent)) {
                            uiScale = c->GetScaleFactor();
                            break;
                        }
                        parent = parent->GetParent();
                    }
                }
                
                UIRect uiRect = m_SelectedUIElement->GetCachedRect();

                ImVec2 pMin = {
                    viewStart.x + (uiRect.X * uiScale),
                    viewStart.y + (uiRect.Y * uiScale)
                };
                ImVec2 pMax = {
                    pMin.x + (uiRect.Width * uiScale),
                    pMin.y + (uiRect.Height * uiScale)
                };

                auto* drawList = ImGui::GetWindowDrawList();

                drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);

                UIRect parentRect = {0,0, viewportSize.x, viewportSize.y};
                if (auto parent = m_SelectedUIElement->GetParent())
                {
                    parentRect = parent->GetCachedRect();
                }
                else
                {
                    if (auto canvas = std::dynamic_pointer_cast<UICanvas>(m_SelectedUIElement))
                    {
                        parentRect = canvas->GetCachedRect();
                    }
                }

                UIAnchor anchor = m_SelectedUIElement->GetAnchor();

                float axMin = parentRect.X + parentRect.Width * anchor.MinX;
                float axMax = parentRect.X + parentRect.Width * anchor.MaxX;
                float ayMin = parentRect.Y + parentRect.Height * anchor.MinY;
                float ayMax = parentRect.Y + parentRect.Height * anchor.MaxY;
                ImVec2 anchorMin = {
                    viewStart.x + (axMin * uiScale),
                    viewStart.y + (ayMin * uiScale)
                };
                ImVec2 anchorMax = {
                    viewStart.x + (axMax * uiScale),
                    viewStart.y + (ayMax * uiScale)
                };

                float size = 6.0f;
                auto col = IM_COL32(255, 50, 50, 255);

                auto DrawCross = [&](ImVec2 center) {
                    drawList->AddLine({center.x - size, center.y - size}, {center.x + size, center.y + size}, col, 2.0f);
                    drawList->AddLine({center.x - size, center.y + size}, {center.x + size, center.y - size}, col, 2.0f);
                };

                DrawCross(anchorMin);
                if (glm::epsilonNotEqual(anchorMin.x, anchorMax.x, glm::epsilon<float>()) || glm::epsilonNotEqual(anchorMin.y, anchorMax.y, glm::epsilon<float>()))
                {
                    DrawCross(anchorMax);
                    // Draw Anchor Box (Dashed line?)
                    drawList->AddRect(anchorMin, anchorMax, IM_COL32(255, 50, 50, 100));
                }

                glm::vec2 pivot = m_SelectedUIElement->GetPivot();
                float px = uiRect.X + (uiRect.Width * pivot.x);
                float py = uiRect.Y + (uiRect.Height * pivot.y);
                ImVec2 pivotPos = {
                    viewStart.x + (px * uiScale),
                    viewStart.y + (py * uiScale)
                };

                auto pivotCol = IM_COL32(50, 150, 255, 255); // Blue
                drawList->AddCircleFilled(pivotPos, 4.0f, pivotCol);
                drawList->AddCircle(pivotPos, 6.0f, pivotCol, 0, 1.5f);
            }
        }

        if (m_Selection != entt::null && m_CurrentGizmoOperation != -1 && Engine().Get().GetSceneState() != SceneState::Play)
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
            auto& tc = scene->Reg().get<TransformComponent>(m_Selection);
            auto& rel = scene->Reg().get<RelationshipComponent>(m_Selection);
            glm::mat4 transform = tc.WorldMatrix;

            bool snap = ImGui::GetIO().KeyCtrl;
            float snapValue = 0.5f;
            if (m_CurrentGizmoOperation == ImGuizmo::ROTATE)
                snapValue = 45.0f;

            float snapValues[3] = { snapValue, snapValue, snapValue };

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_CurrentGizmoOperation,
                ImGuizmo::MODE::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

            if (ImGuizmo::IsUsing())
            {
                glm::mat4 localMatrix = transform;
                if (rel.Parent != entt::null)
                {
                    auto& parentTC = scene->Reg().get<TransformComponent>(rel.Parent);
                    localMatrix = glm::inverse(parentTC.WorldMatrix) * transform;
                }
                
                float matrixTranslation[3], matrixRotation[3], matrixScale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), matrixTranslation, matrixRotation, matrixScale);

                tc.Translation = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
                tc.SetRotationEuler(glm::radians(glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2])));
                tc.Scale = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
            }
        }

        auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
        auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        auto viewportOffset = ImGui::GetWindowPos();

        m_Bounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
        m_Bounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

        if (m_IsFocused && !ImGuizmo::IsUsing())
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                auto mousePos = ImGui::GetMousePos();
                mousePos.x -= m_Bounds[0].x;
                mousePos.y -= m_Bounds[0].y;

                glm::vec2 viewportSize2 = { m_Bounds[1].x - m_Bounds[0].x, m_Bounds[1].y - m_Bounds[0].y };

                int mouseX = (int)mousePos.x;
                int mouseY = (int)mousePos.y;

                if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize2.x && mouseY < (int)viewportSize2.y)
                {
                    int selectedID = renderer.ReadIdFromBuffer(mouseX, mouseY);
                    if (OnSelectionChangedCallback)
                    {
                        if (selectedID >= 0)
                        {
                            OnSelectionChangedCallback((entt::entity)selectedID);
                        }
                        else
                        {
                            OnSelectionChangedCallback(entt::null);
                        }
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

    void Viewport::OnSelectedEntityChanged(entt::entity selectedEntity)
    {
        m_Selection = selectedEntity;
    }

    void Viewport::OnSelectedUIElementChanged(std::shared_ptr<UIElement> uiElement)
    {
        m_SelectedUIElement = uiElement;
    }
}
