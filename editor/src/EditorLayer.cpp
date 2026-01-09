#include "EditorLayer.h"

#include <imgui.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/epsilon.hpp>

#include "Lynx/Scene/SceneSerializer.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/AssetPropertiesPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/RenderSettings.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/StatsPanel.h"
#include "Panels/Viewport.h"
#include "Utils/PlatformUtils.h"

#include "Lynx/Event/KeyEvent.h"
#include "Lynx/Event/MouseEvent.h"
#include "Lynx/Event/SceneEvents.h"
#include "Lynx/Event/WindowEvent.h"

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
        auto uiCallback = [this](std::shared_ptr<UIElement> uiElement)
        {
            this->OnSelectedUIElementChanged(uiElement);
        };
        m_Panels.push_back(std::make_unique<SceneHierarchyPanel>(entityCallack, uiCallback));
        m_Panels.push_back(std::make_unique<InspectorPanel>());

        auto viewport = std::make_unique<Viewport>(entityCallack);
        m_ViewportPanel = viewport.get();
        m_Panels.push_back(std::move(viewport));
        m_Panels.push_back(std::make_unique<AssetBrowserPanel>(assetCallack));
        m_Panels.push_back(std::make_unique<StatsPanel>());
        m_Panels.push_back(std::make_unique<RenderSettings>());
        m_Panels.push_back(std::make_unique<AssetPropertiesPanel>());

        m_Engine->SetAppEventCallback([this](Event& e) { OnEvent(e); });

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

            if (ImGui::MenuItem("Show UI", nullptr, &m_ShowUI))
            {
                m_Engine->GetRenderer().SetShowUI(m_ShowUI);
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
        
        if (m_ViewportPanel)
        {
            glm::vec2 size = m_ViewportPanel->GetSize();
            if (size.x > 0 && size.y > 0 && 
                (glm::epsilonNotEqual(size.x, (float)m_LastViewportWidth, glm::epsilon<float>()) 
                || glm::epsilonNotEqual(size.y, (float)m_LastViewportHeight, glm::epsilon<float>())))
            {
                m_LastViewportWidth = (uint32_t)size.x;
                m_LastViewportHeight = (uint32_t)size.y;
                
                ViewportResizeEvent e(m_LastViewportWidth, m_LastViewportHeight);
                m_Engine->OnEvent(e);
            }
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

        m_Engine->SetSceneState(SceneState::Play);
    }

    void EditorLayer::OnSceneStop()
    {
        m_SelectedEntity = entt::null;
        m_Engine->SetSceneState(SceneState::Edit);
        m_Engine->SetActiveScene(m_EditorScene);
        m_RuntimeScene = nullptr;

        m_Engine->GetRenderer().SetShowUI(m_ShowUI);
    }

    void EditorLayer::OnEvent(Event& e)
    {
        if (!m_ViewportPanel)
            return;

        EventDispatcher dispatcher(e);

        dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e)
        {
            glm::vec2 viewportPos = m_ViewportPanel->GetPos();
            glm::vec2 viewportSize = m_ViewportPanel->GetSize();
            float mouseX = e.GetX();
            float mouseY = e.GetY();

            if (mouseX >= viewportPos.x && mouseX <= viewportPos.x + viewportSize.x &&
                mouseY >= viewportPos.y && mouseY <= viewportPos.y + viewportSize.y)
            {
                float localX = mouseX - viewportPos.x;
                float localY = mouseY - viewportPos.y;

                MouseMovedEvent localE(localX, localY);
                if (m_Engine->GetActiveScene())
                    m_Engine->GetActiveScene()->OnEvent(localE);

            }
            return false;
        });

        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)
        {
            if (m_ViewportPanel->IsHovered())
            {
                if (m_Engine->GetActiveScene())
                    m_Engine->GetActiveScene()->OnEvent(e);
            }
            return false;
        });

        dispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& e)
        {
            if (m_ViewportPanel->IsHovered())
            {
                if (m_Engine->GetActiveScene())
                    m_Engine->GetActiveScene()->OnEvent(e);
            }
            return false;
        });
        
        dispatcher.Dispatch<ActiveSceneChangedEvent>([this](ActiveSceneChangedEvent& e)
        {
            this->OnSceneContextChanged(e.GetScene().get());
            return false;
        });
    }

    void EditorLayer::NewScene()
    {
        m_EditorScene = std::make_shared<Scene>();
        m_Engine->SetActiveScene(m_EditorScene);
        m_SelectedEntity = entt::null;
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
            m_Engine->SetActiveScene(m_EditorScene);
            m_SelectedEntity = entt::null;
        }
    }

    void EditorLayer::OpenScene(AssetHandle handle)
    {
        m_EditorScene = Engine::Get().GetAssetManager().GetAsset<Scene>(handle, AssetLoadMode::Blocking);
        m_Engine->SetActiveScene(m_EditorScene);
        m_SelectedEntity = entt::null;
    }

    void EditorLayer::OnSceneContextChanged(Scene* scene)
    {
        OnSelectedEntityChanged(entt::null);
        OnSelectedUIElementChanged(nullptr);
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

    void EditorLayer::OnSelectedUIElementChanged(std::shared_ptr<UIElement> uiElement)
    {
        m_SelectedUIElement = uiElement;
        for (auto& panel : m_Panels)
            panel->OnSelectedUIElementChanged(uiElement);
    }
}
