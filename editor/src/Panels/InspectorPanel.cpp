#include "InspectorPanel.h"
#include <imgui.h>

#include "Lynx/Asset/Prefab.h"
#include "Lynx/ImGui/LXUI.h"
#include "Lynx/Scene/SceneSerializer.h"
#include "Lynx/Scene/Components/Components.h"

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
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Tag");
                ImGui::SameLine();
                if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                {
                    tag = std::string(buffer);
                }
                
                ImGui::SameLine();
                
                bool isEnabled = !(m_Context->Reg().all_of<DisabledComponent>(m_Selection));
                if (ImGui::Checkbox("##Disabled", &isEnabled))
                {
                    if (isEnabled)
                        m_Context->Reg().remove<DisabledComponent>(m_Selection);
                    else
                        m_Context->Reg().emplace<DisabledComponent>(m_Selection);
                }
            }
            
            if (m_Context->Reg().all_of<PrefabComponent>(m_Selection))
            {
                auto& pc = m_Context->Reg().get<PrefabComponent>(m_Selection);
                if (pc.PrefabHandle.IsValid())
                {
                    ImGui::Text("Prefab Instance"); 
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Overrides"))
                    {
                        Entity root = { m_Selection, m_Context };
                        while (true)
                        {
                            Entity parent = root.GetParent();
                            if (!parent)
                                break;
                            if (!parent.HasComponent<PrefabComponent>())
                                break;
                            if (parent.GetComponent<PrefabComponent>().PrefabHandle != pc.PrefabHandle)
                                break;
                            
                            root = parent;
                        }
                        
                        nlohmann::json prefabJson;
                        SceneSerializer::SerializePrefab(root, prefabJson, true);
                        std::filesystem::path filePath = Engine::Get().GetAssetManager().GetAssetPath(pc.PrefabHandle);
                        std::ofstream fout(filePath);
                        fout << prefabJson.dump(4);
                        fout.close();
                        // TODO: reload asset and replace unedited prefabs in scene with this!
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Revert"))
                    {
                        Entity root = { m_Selection, m_Context };
                        while (true)
                        {
                            Entity parent = root.GetParent();
                            if (!parent)
                                break;
                            if (!parent.HasComponent<PrefabComponent>())
                                break;
                            if (parent.GetComponent<PrefabComponent>().PrefabHandle != pc.PrefabHandle)
                                break;
                            
                            root = parent;
                        }
                        
                        auto prefabAsset = Engine::Get().GetAssetManager().GetAsset<Prefab>(pc.PrefabHandle);
                        auto prefabJson = prefabAsset->GetData();
                        SceneSerializer::DeserializePrefabInto(m_Context, prefabJson, root);
                    }
                }
            }

            if (m_Context->Reg().all_of<TransformComponent>(m_Selection))
            {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    LXUI::BeginPropertyGrid();
                    registeredComponents.at("Transform").drawUI(m_Context->Reg(), m_Selection);
                    LXUI::EndPropertyGrid();
                }
            }

            for (const auto& [name, info] : registeredComponents)
            {
                if (name == "Tag" || name == "Transform" || name == "Relationship" || name == "Prefab")
                    continue;
                
                bool internalUse = info.InternalUseOnly;
                if (!info.drawUI && internalUse)
                    continue;
                
                if (info.has(m_Context->Reg(), m_Selection))
                {
                    ImGui::PushID(name.c_str());
                    bool open = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

                    if (!internalUse && ImGui::BeginPopupContextItem("ComponentSettings"))
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
                    
                    ImGui::PopID();

                    if (open)
                    {
                        if (info.drawUI)
                        {
                            LXUI::BeginPropertyGrid();
                            ImGui::PushID(name.c_str());
                            info.drawUI(m_Context->Reg(), m_Selection);
                            ImGui::PopID();
                            LXUI::EndPropertyGrid();
                        }
                        else
                        {
                            ImGui::BeginDisabled(true);
                            ImGui::Text("No Editor UI implemented.");
                            ImGui::EndDisabled();
                        }
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("Add Component");

            if (ImGui::BeginPopup("Add Component"))
            {
                for (const auto& [name, info] : registeredComponents)
                {
                    if (!info.InternalUseOnly && !info.has(m_Context->Reg(), m_Selection))
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
        else if (m_Context && m_SelectedUIElement)
        {
            LXUI::BeginPropertyGrid();
            m_SelectedUIElement->OnInspect();
            LXUI::EndPropertyGrid();
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

    void InspectorPanel::OnSelectedUIElementChanged(std::shared_ptr<UIElement> uiElement)
    {
        m_SelectedUIElement = uiElement;
    }
}
