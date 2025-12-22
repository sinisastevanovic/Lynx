#include "SceneHierarchyPanel.h"

#include <imgui.h>

#include "../EditorLayer.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Components/Components.h"

namespace Lynx
{
    SceneHierarchyPanel::SceneHierarchyPanel(EditorLayer* owner)
        : m_Owner(owner)
    {
    }

    SceneHierarchyPanel::SceneHierarchyPanel(const std::shared_ptr<Scene>& context)
    {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const std::shared_ptr<Scene>& context)
    {
        m_Context = context;
        //m_SelectedEntity = entt::null;
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            for (auto entity : m_Context->Reg().view<entt::entity>())
            {
                DrawEntityNode(entity, m_Owner->GetSelectedEntity() == entity);
            }

            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem("Create Empty Entity"))
                    m_Context->CreateEntity("Empty Entity");

                ImGui::EndPopup();
            }

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
                m_Owner->SetSelectedEntity(entt::null);
        }
        
        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(entt::entity entity, bool isSelected)
    {
        auto& tag = m_Context->Reg().get<TagComponent>(entity).Tag;

        ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
        if (ImGui::IsItemClicked())
        {
            m_Owner->SetSelectedEntity(entity);
        }

        if (opened)
        {
            // TODO: Draw children
            ImGui::TreePop();
        }
    }
}
