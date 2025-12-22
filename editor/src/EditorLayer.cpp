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
    }

    void EditorLayer::DrawMenuBar()
    {
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
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);

        SceneState state = Engine::Get().GetSceneState();
        const char* label = (state == SceneState::Edit) ? "Play" : "Stop";
        if (ImGui::Button(label))
        {
            if (state == SceneState::Edit)
                OnScenePlay();
            else
                OnSceneStop();
        }
        
        ImGui::End();
    }

    void EditorLayer::OnImGuiRender()
    {
        DrawToolBar();
        m_Viewport.OnImGuiRender();
        m_SceneHierarchyPanel.OnImGuiRender();
        m_InspectorPanel.OnImGuiRender(m_Engine->GetActiveScene(),m_Engine->ComponentRegistry);
    }

    void EditorLayer::OnScenePlay()
    {
        m_SelectedEntity = entt::null;
        SceneSerializer serializer(m_EditorScene);
        std::string data = serializer.SerializeToString();

        m_RuntimeScene = std::make_shared<Scene>();
        SceneSerializer runtimeSerializer(m_RuntimeScene);
        runtimeSerializer.DeserializeFromString(data);

        m_Engine->SetActiveScene(m_RuntimeScene);

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

            SceneSerializer serializer(m_EditorScene);
            if (serializer.Deserialize(filepath))
            {
                m_Engine->SetActiveScene(m_EditorScene);
                m_SelectedEntity = entt::null;
                m_SceneHierarchyPanel.SetContext(m_Engine->GetActiveScene());
            }
        }
    }
}
