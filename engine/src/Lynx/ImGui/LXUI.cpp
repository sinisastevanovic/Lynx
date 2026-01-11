#include "LXUI.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    void LXUI::DrawLabel(const std::string& label)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label.c_str());
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);
    }

    bool LXUI::PropertyGroup(const std::string& name, bool autoOpen)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool open = ImGui::CollapsingHeader(name.c_str(), autoOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        if (open)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
        }
        return open;
    }

    void LXUI::BeginPropertyGrid()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        static ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV |
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame;

        if (ImGui::BeginTable("Properties", 2, flags))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
        }
    }

    void LXUI::EndPropertyGrid()
    {
        ImGui::EndTable();
        ImGui::PopStyleVar(2);
    }
    
    bool LXUI::DrawDragFloat(const std::string& label, float& value, float speed, float min, float max, float resetValue)
    {
        DrawLabel(label);
        std::string id = "##" + label;
        return ImGui::DragFloat(id.c_str(), &value, speed, min, max);
    }

    bool LXUI::DrawSliderFloat(const std::string& label, float& value, float min, float max, float resetValue)
    {
        DrawLabel(label);
        std::string id = "##" + label;
        return ImGui::SliderFloat(id.c_str(), &value, min, max, "%.2f");
    }

    bool LXUI::DrawDragInt(const std::string& label, int& value, float speed, int min, int max, int resetValue)
    {
        DrawLabel(label);
        std::string id = "##" + label;
        return ImGui::DragInt(id.c_str(), &value, speed, min, max);
    }

    bool LXUI::DrawCheckBox(const std::string& label, bool& value)
    {
        DrawLabel(label);
        std::string id = "##" + label;
        return ImGui::Checkbox(id.c_str(), &value);
    }

    bool LXUI::DrawTextInput(const std::string& label, std::string& value)
    {
        DrawLabel(label);
        std::string id = "##" + label;

        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strcpy_s(buffer, sizeof(buffer), value.c_str());

        if (ImGui::InputText(id.c_str(), buffer, sizeof(buffer)))
        {
            value = std::string(buffer);
            return true;
        }
        return false;
    }

    bool LXUI::DrawTextInputMultiline(const std::string& label, std::string& value)
    {
        DrawLabel(label);
        std::string id = "##" + label;

        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strcpy_s(buffer, sizeof(buffer), value.c_str());

        if (ImGui::InputTextMultiline(id.c_str(), buffer, sizeof(buffer)))
        {
            value = std::string(buffer);
            return true;
        }
        return false;
    }

    bool LXUI::DrawColorControl(const std::string& label, glm::vec4& value)
    {
        DrawLabel(label);
        std::string id = "##" + label;
        return ImGui::ColorEdit4(id.c_str(), &value.r);
    }

    bool LXUI::DrawColor3Control(const std::string& label, glm::vec3& value)
    {
        DrawLabel(label);
        std::string id = "##" + label;
        return ImGui::ColorEdit3(id.c_str(), &value.r);
    }

    bool LXUI::DrawVec2Control(const std::string& label, glm::vec2& value, float speed, float min, float max, float resetValue)
    {
        DrawLabel(label);
        
        ImGui::PushID(label.c_str());
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
        
        float totalButtonWidth = buttonSize.x * 2.0f;
        float widthForDrags = availableWidth - totalButtonWidth;
        
        bool changed = false;
        
        ImGui::PushMultiItemsWidths(2, widthForDrags);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0));
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        if (ImGui::Button("X", buttonSize))
        {
            value.x = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        if (ImGui::DragFloat("##X", &value.x, speed, min, max, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        if (ImGui::Button("Y", buttonSize))
        {
            value.y = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::DragFloat("##Y", &value.y, speed, min, max, "%.2f")) changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PopStyleVar();

        ImGui::PopID();
        return changed;
    }
    
    bool LXUI::DrawVec3Control(const std::string& label, glm::vec3& value, float min, float max, float resetValue)
    {
        DrawLabel(label);
        
        ImGui::PushID(label.c_str());
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
        
        float totalButtonWidth = buttonSize.x * 3.0f;
        float widthForDrags = availableWidth - totalButtonWidth;
        
        bool changed = false;
        
        ImGui::PushMultiItemsWidths(3, widthForDrags);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0));
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        if (ImGui::Button("X", buttonSize))
        {
            value.x = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        if (ImGui::DragFloat("##X", &value.x, 0.1f, min, max, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        if (ImGui::Button("Y", buttonSize))
        {
            value.y = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::DragFloat("##Y", &value.y, 0.1f, min, max, "%.2f")) changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Z Axis (Blue)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        if (ImGui::Button("Z", buttonSize))
        {
            value.z = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::DragFloat("##Z", &value.z, 0.1f, min, max, "%.2f")) changed = true;
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();

        ImGui::PopID();
        return changed;
    }
    
    bool LXUI::DrawVec4Control(const std::string& label, glm::vec4& value, float min, float max, float resetValue)
    {
        DrawLabel(label);
        
        ImGui::PushID(label.c_str());
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
        
        float totalButtonWidth = buttonSize.x * 4.0f;
        float widthForDrags = availableWidth - totalButtonWidth;
        
        bool changed = false;
        
        ImGui::PushMultiItemsWidths(4, widthForDrags);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0));
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        if (ImGui::Button("X", buttonSize))
        {
            value.x = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        if (ImGui::DragFloat("##X", &value.x, 0.1f, min, max, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        if (ImGui::Button("Y", buttonSize))
        {
            value.y = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::DragFloat("##Y", &value.y, 0.1f, min, max, "%.2f")) changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Z Axis (Blue)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        if (ImGui::Button("Z", buttonSize))
        {
            value.z = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::DragFloat("##Z", &value.z, 0.1f, min, max, "%.2f")) changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        // 2 Axis (Grey)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.4f, 0.4f, 0.4f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.4f, 0.4f, 0.4f, 1.0f });
        if (ImGui::Button("W", buttonSize))
        {
            value.w = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::DragFloat("##W", &value.w, 0.1f, min, max, "%.2f")) changed = true;
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();

        ImGui::PopID();
        return changed;
    }

    bool LXUI::DrawComboControl(const std::string& label, int& currentItem, const std::vector<std::string>& items)
    {
        DrawLabel(label);
        std::string id = "##" + label;

        const char* preview = items.size() > 0 ? items[currentItem].c_str() : "";
        bool changed = false;

        if (ImGui::BeginCombo(id.c_str(), preview))
        {
            for (int i = 0; i < items.size(); i++)
            {
                bool isSelected = (currentItem == i);
                if (ImGui::Selectable(items[i].c_str(), isSelected))
                {
                    currentItem = i;
                    changed = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool LXUI::DrawAssetReference(const std::string& label, AssetHandle& currentHandle, std::initializer_list<AssetType> allowedTypes)
    {
        DrawLabel(label);
        
        bool changed = false;
        std::string currentName = "None";
        if (currentHandle.IsValid())
        {
            currentName = Engine::Get().GetAssetManager().GetAssetName(currentHandle);
        }

        ImGui::PushID(label.c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0));
        if (ImGui::Button(currentName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - (32.0f + 4.0f), 0.0f)))
        {
            ImGui::OpenPopup("AssetSelectPopup");
        }

        if (ImGui::BeginDragDropTarget())
        {
            for (AssetType type : allowedTypes)
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetUtils::GetDragDropPayload(type)))
                {
                    uint64_t data = *(const uint64_t*)payload->Data;
                    currentHandle = AssetHandle(data);
                    changed = true;
                    break;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(32.0f, 0.0f)))
        {
            currentHandle = AssetHandle::Null();
            changed = true;
        }
        
        ImGui::PopStyleVar();

        if (ImGui::BeginPopup("AssetSelectPopup"))
        {
            static char searchBuffer[128] = "";
            ImGui::InputTextWithHint("##Search", "Serach...", searchBuffer, sizeof(searchBuffer));

            ImGui::Separator();

            if (ImGui::Selectable("None"))
            {
                currentHandle = AssetHandle::Null();
                changed = true;
                ImGui::CloseCurrentPopup();
            }

            std::string searchQuery = searchBuffer;
            std::ranges::transform(searchQuery, searchQuery.begin(), ::tolower);
            const auto& allAssets = Engine::Get().GetAssetRegistry().GetMetadata();
            int assetIndex = 0;
            for (const auto& [handle, meta] : allAssets)
            {
                bool typeAllowed = false;
                for (AssetType t : allowedTypes)
                {
                    if (meta.Type == t)
                    {
                        typeAllowed = true;
                        break;
                    }
                }
                
                if (!typeAllowed)
                    continue;


                std::string assetName = meta.FilePath.filename().string();
                std::string assetNameLower = assetName;
                std::ranges::transform(assetNameLower, assetNameLower.begin(), ::tolower);

                if (!searchQuery.empty() && assetNameLower.find(searchQuery) == std::string::npos)
                    continue;

                ImGui::PushID(assetIndex);
                bool isSelected = (currentHandle == handle);
                if (ImGui::Selectable(assetName.c_str(), isSelected))
                {
                    currentHandle = handle;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                assetIndex++;
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    bool LXUI::DrawUIElementSelection(const std::string& label, UUID& id, Scene* context)
    {
        DrawLabel(label);
        
        ImGui::PushID(label.c_str());
        std::string name = "None";
        if (id.IsValid() && context)
        {
            auto element = context->FindUIElementByID(id);
            if (element)
                name = element->GetName();
            else
                name = "Invalid (" + std::to_string((uint64_t)id) + ")";
        }
        
        bool changed = false;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0));
        ImGui::Button(name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - (32.0f + 4.0f), 0.0f));
        
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_UI_ELEMENT"))
            {
                UIElement* ptr = *(UIElement**)payload->Data;
                id = ptr->GetUUID();
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(32.0f, 0.0f)))
        {
            id = UUID::Null();
            changed = true;
        }

        ImGui::PopStyleVar();
        ImGui::PopID();
        return changed;
    }

    void LXUI::DrawLuaScriptSection(LuaScriptComponent* lsc)
    {
        if (!lsc)
            return;

        if (lsc->Self.valid())
        {
            std::optional<sol::table> propsOpt = lsc->Self["Properties"];
            if (propsOpt)
            {
                sol::table propsDef = propsOpt.value();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::TreeNodeEx("Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    
                    for (auto& [key, value] : propsDef)
                    {
                        if (!key.is<std::string>())
                            continue;

                        std::string name = key.as<std::string>();
                        sol::table def = value.as<sol::table>();
                        std::string type = def["Type"];

                        ImGui::PushID(name.c_str());

                        if (type == "Float")
                        {
                            float val = lsc->Self[name].get_or(def["Default"].get<float>());
                            if (DrawDragFloat(name, val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Int")
                        {
                            int val = lsc->Self[name].get_or(def["Default"].get<int>());
                            if (DrawDragInt(name, val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Bool")
                        {
                            bool val = lsc->Self[name].get_or(def["Default"].get<bool>());
                            if (DrawCheckBox(name, val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "String")
                        {
                            
                            std::string val = lsc->Self[name].get_or(def["Default"].get<std::string>());
                            if (DrawTextInput(name, val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Vec2")
                        {
                            glm::vec2 val = lsc->Self[name].get_or(def["Default"].get<glm::vec2>());
                            if (DrawVec2Control(name.c_str(), val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Vec3")
                        {
                            glm::vec3 val = lsc->Self[name].get_or(def["Default"].get<glm::vec3>());
                            if (DrawVec3Control(name.c_str(), val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Color")
                        {
                            glm::vec4 val = lsc->Self[name].get_or(def["Default"].get<glm::vec4>());
                            if (DrawColorControl(name.c_str(), val))
                            {
                                lsc->Self[name] = val;
                            }
                        }

                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }
        }
    }

    bool LXUI::DrawUIAnchorControl(const std::string& label, UIAnchor& anchor)
    {
        DrawLabel(label);
        
        ImGui::PushID(label.c_str());
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
        
        float totalButtonWidth = buttonSize.x * 4.0f;
        float widthForDrags = availableWidth - totalButtonWidth;
        
        bool changed = false;
        
        ImGui::PushMultiItemsWidths(4, widthForDrags);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0));

        if (ImGui::Button("L", buttonSize))
        {
            anchor.MinX = 0.5f;
            changed = true;
        }
        
        ImGui::SameLine();
        if (ImGui::DragFloat("##L", &anchor.MinX, 0.01f, 0, 1, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("T", buttonSize))
        {
            anchor.MinY = 0.5f;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::DragFloat("##T", &anchor.MinY, 0.01f, 0, 1, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("R", buttonSize))
        {
            anchor.MaxX = 0.5f;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::DragFloat("##R", &anchor.MaxX, 0.01f, 0, 1, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("B", buttonSize))
        {
            anchor.MaxY = 0.5f;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::DragFloat("##B", &anchor.MaxY, 0.01f, 0, 1, "%.2f")) 
            changed = true;
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        if (ImGui::Button("Stretch All"))
        {
            anchor = UIAnchor::StretchAll;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool LXUI::DrawUIThicknessControl(const std::string& label, UIThickness& thickness)
    {
        DrawLabel(label);
        
        ImGui::PushID(label.c_str());
        
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
        
        float totalButtonWidth = buttonSize.x * 4.0f;
        float widthForDrags = availableWidth - totalButtonWidth;
        
        bool changed = false;
        
        ImGui::PushMultiItemsWidths(4, widthForDrags);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0));
        
        if (ImGui::Button("L", buttonSize))
        {
            thickness.Left = 0.0f;
            changed = true;
        }
        
        ImGui::SameLine();
        if (ImGui::DragFloat("##L", &thickness.Left, 1.0f, 0, 0, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        if (ImGui::Button("T", buttonSize))
        {
            thickness.Top = 0.0f;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::DragFloat("##T", &thickness.Top, 1.0f, 0, 0, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("R", buttonSize))
        {
            thickness.Right = 0.0f;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::DragFloat("##R", &thickness.Right, 1.0f, 0, 0, "%.2f"))
            changed = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        
        if (ImGui::Button("B", buttonSize))
        {
            thickness.Bottom = 0.0f;
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::DragFloat("##B", &thickness.Bottom, 1.0f, 0, 0, "%.2f")) 
            changed = true;
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();


        ImGui::PopID();
        return changed;
    }
}
