#include "Viewport.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "../EditorLayer.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

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

        if (ImGui::IsWindowFocused())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_CurrentGizmoOperation = -1;
            if (ImGui::IsKeyPressed(ImGuiKey_W)) m_CurrentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_CurrentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_CurrentGizmoOperation = ImGuizmo::SCALE;
        }

        SceneState state = Engine::Get().GetSceneState();
        const char* label = (state == SceneState::Edit) ? "Play" : "Stop";
        if (ImGui::Button(label))
        {
            if (state == SceneState::Edit)
                m_Owner->OnScenePlay();
            else
                m_Owner->OnSceneStop();
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
        if (selectedEntity != entt::null && m_CurrentGizmoOperation != -1)
        {
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
        
        ImGui::End();
    }
}
