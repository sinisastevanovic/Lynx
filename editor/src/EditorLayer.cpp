#include "EditorLayer.h"

#include <imgui.h>

#include "Lynx/Scene/SceneSerializer.h"
#include "Utils/PlatformUtils.h"

namespace Lynx
{
    EditorLayer::EditorLayer(Engine* engine)
        : m_Engine(engine), m_SceneHierarchyPanel(this), m_InspectorPanel(this)
    {
        m_EditorScene = engine->GetActiveScene();
    }

    void EditorLayer::OnAttach()
    {
        m_Viewport.SetOwner(this);
        m_SceneHierarchyPanel.SetContext(m_Engine->GetActiveScene());
    }

    void EditorLayer::OnDetach()
    {
        m_EditorScene.reset();
        m_RuntimeScene.reset();
    }

    void EditorLayer::DrawMenuBar()
    {
        // TODO: Disable these in runtime.
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                {
                    SaveSceneAs();
                }
                if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
                {
                    OpenScene();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void EditorLayer::DrawToolBar()
    {
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        SceneState state = Engine::Get().GetSceneState();
        const char* label = (state == SceneState::Edit) ? "Play" : "Stop";
        if (ImGui::Button(label))
        {
            if (state == SceneState::Edit)
                OnScenePlay();
            else
                OnSceneStop();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        auto& colors = ImGui::GetStyle().Colors;
        const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
        const auto& buttonActive = colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 100.0f); // Align to right? Or keep left.

        if (ImGui::Button("View Options"))
        {
            ImGui::OpenPopup("ViewOptionsPopup");
        }

        if (ImGui::BeginPopup("ViewOptionsPopup"))
        {
            ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
            bool showGrid = m_Engine->GetRenderer().GetShowGrid();
            if (ImGui::MenuItem("Show Grid", nullptr, &showGrid))
            {
                m_Engine->GetRenderer().SetShowGrid(showGrid);
            }
            /*if (ImGui::Selectable("Show Grid", &showGrid, ImGuiSelectableFlags_DontClosePopups))
            {
                m_Engine->GetRenderer().SetShowGrid(showGrid);
            }*/

            bool showColliders = m_Engine->GetRenderer().GetShowColliders();
            if (ImGui::MenuItem("Show Colliders", nullptr, &showColliders))
            {
                m_Engine->GetRenderer().SetShowColliders(showColliders);
            }

            /*if (ImGui::Selectable("Show Colliders", &showColliders, ImGuiSelectableFlags_DontClosePopups))
            {
                m_Engine->GetRenderer().SetShowColliders(showColliders);
            }*/
            ImGui::PopItemFlag();
            ImGui::EndPopup();
        }

        //ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::End();
    }

    void EditorLayer::OnImGuiRender()
    {
        DrawToolBar();
        m_Viewport.OnImGuiRender();
        m_SceneHierarchyPanel.OnImGuiRender();
        m_InspectorPanel.OnImGuiRender(m_Engine->GetActiveScene(),m_Engine->ComponentRegistry);
        m_AssetBrowserPanel.OnImGuiRender();
        m_StatsPanel.OnImGuiRender();
    }

    void EditorLayer::OnScenePlay()
    {
        m_Engine->GetScriptEngine()->OnEditorEnd();
        
        m_SelectedEntity = entt::null;
        SceneSerializer serializer(m_EditorScene);
        std::string data = serializer.SerializeToString();

        m_RuntimeScene = std::make_shared<Scene>();
        m_Engine->SetActiveScene(m_RuntimeScene);
        SceneSerializer runtimeSerializer(m_RuntimeScene);
        runtimeSerializer.DeserializeFromString(data);

        m_SceneHierarchyPanel.SetContext(m_RuntimeScene);

        m_Engine->SetSceneState(SceneState::Play);
    }

    void EditorLayer::OnSceneStop()
    {
        m_SelectedEntity = entt::null;
        m_Engine->SetSceneState(SceneState::Edit);
        m_Engine->SetActiveScene(m_EditorScene);
        m_SceneHierarchyPanel.SetContext(m_EditorScene);
        m_RuntimeScene = nullptr;

        m_Engine->GetScriptEngine()->OnEditorStart(m_Engine->GetActiveScene().get());
    }

    void EditorLayer::SaveSceneAs()
    {
        std::string filepath = FileDialogs::SaveFile(AssetUtils::GetFilterForAssetType(AssetType::Scene));
        if (!filepath.empty())
        {
            SceneSerializer serializer(m_EditorScene);
            serializer.Serialize(filepath);
        }
    }

    void EditorLayer::OpenScene()
    {
        std::string filepath = FileDialogs::OpenFile(AssetUtils::GetFilterForAssetType(AssetType::Scene));
        if (!filepath.empty())
        {
            m_EditorScene = std::make_shared<Scene>();
            m_EditorScene->OnRuntimeStop();
            m_Engine->SetActiveScene(m_EditorScene);
            m_Engine->GetScriptEngine()->OnEditorStart(m_Engine->GetActiveScene().get());

            SceneSerializer serializer(m_EditorScene);
            if (serializer.Deserialize(filepath))
            {
                m_SelectedEntity = entt::null;
                m_SceneHierarchyPanel.SetContext(m_Engine->GetActiveScene());

            }
        }
    }
}
