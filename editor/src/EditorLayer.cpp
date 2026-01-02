#include "EditorLayer.h"

#include <imgui.h>

#include "Lynx/Scene/SceneSerializer.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/AssetPropertiesPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/RenderSettings.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/StatsPanel.h"
#include "Panels/Viewport.h"
#include "Utils/PlatformUtils.h"

namespace Lynx
{
    EditorLayer::EditorLayer(Engine* engine)
        : m_Engine(engine)
    {
        m_EditorScene = engine->GetActiveScene();
    }

    void EditorLayer::OnAttach()
    {
        auto entityCallack = [this](entt::entity entity)
        {
            this->OnSelectedEntityChanged(entity);
        };
        auto assetCallack = [this](AssetHandle handle)
        {
            this->OnSelectedAssetChanged(handle);
        };
        m_Panels.push_back(std::make_unique<SceneHierarchyPanel>(entityCallack));
        m_Panels.push_back(std::make_unique<InspectorPanel>());
        m_Panels.push_back(std::make_unique<Viewport>(entityCallack));
        m_Panels.push_back(std::make_unique<AssetBrowserPanel>(assetCallack));
        m_Panels.push_back(std::make_unique<StatsPanel>());
        m_Panels.push_back(std::make_unique<RenderSettings>());
        m_Panels.push_back(std::make_unique<AssetPropertiesPanel>());

        OnSceneContextChanged(m_EditorScene.get());
    }

    void EditorLayer::OnDetach()
    {
        m_EditorScene.reset();
        m_RuntimeScene.reset();
        m_Panels.clear();
    }

    void EditorLayer::DrawMenuBar()
    {
        // TODO: Disable these in runtime.
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::BeginDisabled(m_Engine->GetSceneState() == SceneState::Play);
                if (ImGui::MenuItem("New Scene"))
                {
                    NewScene();
                }
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                {
                    SaveScene();
                }
                if (ImGui::MenuItem("Save Scene As"))
                {
                    SaveSceneAs();
                }
                if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
                {
                    OpenScene();
                }
                ImGui::EndDisabled();
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

            bool showColliders = m_Engine->GetRenderer().GetShowColliders();
            if (ImGui::MenuItem("Show Colliders", nullptr, &showColliders))
            {
                m_Engine->GetRenderer().SetShowColliders(showColliders);
            }

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
        for (auto& panel : m_Panels)
        {
            panel->OnImGuiRender();
        }
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

        OnSceneContextChanged(m_RuntimeScene.get());

        m_Engine->SetSceneState(SceneState::Play);
    }

    void EditorLayer::OnSceneStop()
    {
        m_SelectedEntity = entt::null;
        m_Engine->SetSceneState(SceneState::Edit);
        m_Engine->SetActiveScene(m_EditorScene);
        OnSceneContextChanged(m_EditorScene.get());
        m_RuntimeScene = nullptr;

        m_Engine->GetScriptEngine()->OnEditorStart(m_Engine->GetActiveScene().get());
    }

    void EditorLayer::NewScene()
    {
        m_EditorScene = std::make_shared<Scene>();
        m_EditorScene->OnRuntimeStop();
        m_Engine->SetActiveScene(m_EditorScene);
        m_Engine->GetScriptEngine()->OnEditorStart(m_Engine->GetActiveScene().get());
        m_SelectedEntity = entt::null;
        OnSceneContextChanged(m_Engine->GetActiveScene().get());
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

    void EditorLayer::SaveScene()
    {
        std::string filepath = m_EditorScene->GetFilePath();
        if (!filepath.empty())
        {
            SceneSerializer serializer(m_EditorScene);
            serializer.Serialize(filepath);
        }
        else
        {
            SaveSceneAs();
        }
    }

    void EditorLayer::OpenScene()
    {
        std::string filepath = FileDialogs::OpenFile(AssetUtils::GetFilterForAssetType(AssetType::Scene));
        if (!filepath.empty())
        {
            m_EditorScene = Engine::Get().GetAssetManager().GetAsset<Scene>(filepath, AssetLoadMode::Blocking);
            m_EditorScene->OnRuntimeStop();
            m_Engine->SetActiveScene(m_EditorScene);
            m_Engine->GetScriptEngine()->OnEditorStart(m_Engine->GetActiveScene().get());
            m_SelectedEntity = entt::null;
            OnSceneContextChanged(m_Engine->GetActiveScene().get());
        }
    }

    void EditorLayer::OpenScene(AssetHandle handle)
    {
        m_EditorScene = Engine::Get().GetAssetManager().GetAsset<Scene>(handle, AssetLoadMode::Blocking);
        m_EditorScene->OnRuntimeStop();
        m_Engine->SetActiveScene(m_EditorScene);
        m_Engine->GetScriptEngine()->OnEditorStart(m_Engine->GetActiveScene().get());
        m_SelectedEntity = entt::null;
        OnSceneContextChanged(m_Engine->GetActiveScene().get());
    }

    void EditorLayer::OnSceneContextChanged(Scene* scene)
    {
        for (auto& panel : m_Panels)
            panel->OnSceneContextChanged(scene);
    }

    void EditorLayer::OnSelectedEntityChanged(entt::entity entity)
    {
        m_SelectedEntity = entity;
        for (auto& panel : m_Panels)
            panel->OnSelectedEntityChanged(entity);
    }

    void EditorLayer::OnSelectedAssetChanged(AssetHandle asset)
    {
        m_SelectedAsset = asset;
        for (auto& panel : m_Panels)
            panel->OnSelectedAssetChanged(asset);

        if (Engine::Get().GetAssetRegistry().Contains(m_SelectedAsset))
        {
            const auto& metadata = Engine::Get().GetAssetRegistry().Get(m_SelectedAsset);
            if (metadata.Type == AssetType::Scene)
            {
                OpenScene(metadata.Handle);
            }
        }
    }
}
