#include "InspectorPanel.h"
#include <imgui.h>

#include "Lynx/Scene/Components/Components.h"
#include "../EditorLayer.h"

namespace Lynx
{
    void InspectorPanel::OnImGuiRender()
    {
        ImGui::Begin("Inspector");

        if (m_Context && m_Selection != entt::null)
        {
            const auto& registeredComponents = Engine::Get().GetComponentRegistry().GetRegisteredComponents();

            if (m_Context->Reg().all_of<TagComponent>(m_Selection))
            {
                auto& tag = m_Context->Reg().get<TagComponent>(m_Selection).Tag;
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strcpy_s(buffer, sizeof(buffer), tag.c_str());
                if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
                {
                    tag = std::string(buffer);
                }
            }

            if (m_Context->Reg().all_of<TransformComponent>(m_Selection))
            {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    registeredComponents.at("Transform").drawUI(m_Context->Reg(), m_Selection);
                }
            }

            for (const auto& [name, info] : registeredComponents)
            {
                if (name == "Tag" || name == "Transform")
                    continue;
                
                if (info.has(m_Context->Reg(), m_Selection))
                {
                    ImGui::PushID(name.c_str());
                    bool open = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

                    if (ImGui::BeginPopupContextItem("ComponentSettings"))
                    {
                        if (ImGui::MenuItem("Remove Component"))
                        {
                            if (info.remove)
                                info.remove(m_Context->Reg(), m_Selection);

                            ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                            ImGui::PopID();
                            continue;
                        }
                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        info.drawUI(m_Context->Reg(), m_Selection);
                    }
                    
                    ImGui::PopID();
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("Add Component");

            if (ImGui::BeginPopup("Add Component"))
            {
                for (const auto& [name, info] : registeredComponents)
                {
                    if (!info.has(m_Context->Reg(), m_Selection))
                    {
                        if (ImGui::MenuItem(name.c_str()))
                        {
                            info.add(m_Context->Reg(), m_Selection);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    void InspectorPanel::OnSceneContextChanged(Scene* context)
    {
        m_Context = context;
    }

    void InspectorPanel::OnSelectedEntityChanged(entt::entity selectedEntity)
    {
        m_Selection = selectedEntity;
    }
}
