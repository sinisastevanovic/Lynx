#include "SceneHierarchyPanel.h"

#include <imgui.h>

#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Components/Components.h"

namespace Lynx
{
    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            for (auto entity : m_Context->Reg().view<entt::entity>())
            {
                auto& rel = m_Context->Reg().get<RelationshipComponent>(entity);
                if (rel.Parent == entt::null)
                    DrawEntityNode(entity, m_Selection == entity);
            }

            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem("Create Empty Entity"))
                    m_Context->CreateEntity("Empty Entity");

                ImGui::EndPopup();
            }

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && OnSelectionChangedCallback)
                OnSelectionChangedCallback(entt::null);

            ImGui::Dummy(ImGui::GetContentRegionAvail());
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
                {
                    entt::entity payloadEntity = *(const entt::entity*)payload->Data;
                    m_Context->DetachEntityKeepWorld(payloadEntity);
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsWindowFocused() && m_Selection != entt::null)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Delete))
                {
                    m_DeferredDeletion = m_Selection;
                }
            }
        }
        
        ImGui::End();

        if (m_DeferredDeletion != entt::null)
        {
            if (m_Context->Reg().valid(m_DeferredDeletion))
            {
                m_Context->DestroyEntity(m_DeferredDeletion);
                if (m_Selection == m_DeferredDeletion)
                    OnSelectionChangedCallback(entt::null);
            }
            m_DeferredDeletion = entt::null;
        }
    }

    void SceneHierarchyPanel::OnSceneContextChanged(Scene* context)
    {
        m_Context = context;
        m_Selection = entt::null;
    }

    void SceneHierarchyPanel::OnSelectedEntityChanged(entt::entity selectedEntity)
    {
        m_Selection = selectedEntity;
    }

    void SceneHierarchyPanel::DrawEntityNode(entt::entity entity, bool isSelected)
    {
        auto& tag = m_Context->Reg().get<TagComponent>(entity).Tag;

        ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        bool hasChildren = (m_Context->Reg().get<RelationshipComponent>(entity).ChildrenCount > 0);
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete Entity"))
            {
                m_DeferredDeletion = entity;
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("SCENE_HIERARCHY_ENTITY", &entity, sizeof(entt::entity));
            ImGui::Text("%s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
            {
                entt::entity payloadEntity = *(const entt::entity*)payload->Data;
                if (payloadEntity != entity)
                {
                    m_Context->AttachEntityKeepWorld(payloadEntity, entity);
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        if (ImGui::IsItemClicked() && OnSelectionChangedCallback)
        {
            OnSelectionChangedCallback(entity);
        }

        if (opened)
        {
            if (hasChildren)
            {
                auto& rel = m_Context->Reg().get<RelationshipComponent>(entity);
                entt::entity currentChild = rel.FirstChild;

                while (currentChild != entt::null)
                {
                    DrawEntityNode(currentChild, m_Selection == currentChild);
                    currentChild = m_Context->Reg().get<RelationshipComponent>(currentChild).NextSibling;
                }
            }
           
            ImGui::TreePop();
        }
    }
}
