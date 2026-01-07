#include "EditorUIHelpers.h"

#include <imgui.h>
#include "Lynx/Engine.h"

namespace Lynx
{
    bool EditorUIHelpers::DrawAssetSelection(const char* label, AssetHandle& currentHandle, std::initializer_list<AssetType> allowedTypes)
    {
        bool changed = false;

        std::string currentName = "None";
        if (currentHandle.IsValid())
        {
            currentName = Engine::Get().GetAssetManager().GetAssetName(currentHandle);
        }

        ImGui::PushID(label);
        if (ImGui::Button(currentName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 32.0f, 0.0f)))
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
        ImGui::Text(label);

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

                bool isSelected = (currentHandle == handle);
                if (ImGui::Selectable(assetName.c_str(), isSelected))
                {
                    currentHandle = handle;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    void EditorUIHelpers::DrawLuaScriptSection(LuaScriptComponent* lsc)
    {
        if (!lsc)
            return;

        if (lsc->Self.valid())
        {
            std::optional<sol::table> propsOpt = lsc->Self["Properties"];
            if (propsOpt)
            {
                sol::table propsDef = propsOpt.value();
                if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
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
                            if (ImGui::DragFloat(name.c_str(), &val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Int")
                        {
                            int val = lsc->Self[name].get_or(def["Default"].get<int>());
                            if (ImGui::DragInt(name.c_str(), &val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Bool")
                        {
                            bool val = lsc->Self[name].get_or(def["Default"].get<bool>());
                            if (ImGui::Checkbox(name.c_str(), &val))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "String")
                        {
                            std::string val = lsc->Self[name].get_or(def["Default"].get<std::string>());
                            char buf[256];
                            strcpy_s(buf, val.c_str());
                            if (ImGui::InputText(name.c_str(), buf, sizeof(buf)))
                            {
                                lsc->Self[name] = std::string(buf);
                            }
                        }
                        else if (type == "Vec2")
                        {
                            glm::vec2 val = lsc->Self[name].get_or(def["Default"].get<glm::vec2>());
                            if (ImGui::DragFloat2(name.c_str(), &val.x))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Vec3")
                        {
                            glm::vec3 val = lsc->Self[name].get_or(def["Default"].get<glm::vec3>());
                            if (ImGui::DragFloat3(name.c_str(), &val.x))
                            {
                                lsc->Self[name] = val;
                            }
                        }
                        else if (type == "Color")
                        {
                            glm::vec4 val = lsc->Self[name].get_or(def["Default"].get<glm::vec4>());
                            if (ImGui::ColorEdit4(name.c_str(), &val.r))
                            {
                                lsc->Self[name] = val;
                            }
                        }

                        ImGui::PopID();
                    }
                }
            }
        }
    }
}
