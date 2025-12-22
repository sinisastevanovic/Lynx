#include "InspectorPanel.h"
#include <imgui.h>

#include "Lynx/Scene/Components/Components.h"

namespace Lynx
{
    void InspectorPanel::OnImGuiRender(std::shared_ptr<Scene> context, entt::entity selectedEntity, ComponentRegistry& registry)
    {
        ImGui::Begin("Inspector");

        if (context && selectedEntity != entt::null)
        {
            const auto& registeredComponents = registry.GetRegisteredComponents();

            if (context->Reg().all_of<TagComponent>(selectedEntity))
            {
                auto& tag = context->Reg().get<TagComponent>(selectedEntity).Tag;
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strcpy_s(buffer, sizeof(buffer), tag.c_str());
                if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
                {
                    tag = std::string(buffer);
                }
            }

            if (context->Reg().all_of<TransformComponent>(selectedEntity))
            {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    registeredComponents.at("Transform").drawUI(context->Reg(), selectedEntity);
                }
            }

            for (const auto& [name, info] : registeredComponents)
            {
                if (name == "Tag" || name == "Transform")
                    continue;
                
                if (info.has(context->Reg(), selectedEntity))
                {
                    ImGui::PushID(name.c_str());
                    bool open = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

                    if (ImGui::BeginPopupContextItem("ComponentSettings"))
                    {
                        if (ImGui::MenuItem("Remove Component"))
                        {
                            if (info.remove)
                                info.remove(context->Reg(), selectedEntity);

                            ImGui::CloseCurrentPopup();
                            ImGui::EndPopup();
                            ImGui::PopID();
                            continue;
                        }
                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        info.drawUI(context->Reg(), selectedEntity);
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
                    if (!info.has(context->Reg(), selectedEntity))
                    {
                        if (ImGui::MenuItem(name.c_str()))
                        {
                            info.add(context->Reg(), selectedEntity);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
