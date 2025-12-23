#include "EditorUIHelpers.h"

#include <imgui.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    bool EditorUIHelpers::DrawAssetSelection(const char* label, AssetHandle& currentHandle, AssetType typeFilter)
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
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetUtils::GetDragDropPayload(typeFilter)))
            {
                uint64_t data = *(const uint64_t*)payload->Data;
                currentHandle = AssetHandle(data);
                changed = true;
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
                if (meta.Type != typeFilter)
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
}
