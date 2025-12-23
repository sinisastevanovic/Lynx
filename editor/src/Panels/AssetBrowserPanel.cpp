#include "AssetBrowserPanel.h"

#include <imgui.h>

#include <algorithm>

#include "Lynx/Engine.h"

namespace Lynx
{
    AssetBrowserPanel::AssetBrowserPanel()
        : m_BaseDirectory("assets"), m_CurrentDirectory(m_BaseDirectory)
    {
        m_DirectoryIcon = Engine::Get().GetAssetManager().GetAsset<Texture>("engine/resources/Icons/DirectoryIcon_128px.png");
        m_FileIcon = Engine::Get().GetAssetManager().GetAsset<Texture>("engine/resources/Icons/FileIcon_128px.png");
    }

    void AssetBrowserPanel::OnImGuiRender()
    {
        ImGui::Begin("Asset Browser");

        if (m_CurrentDirectory != m_BaseDirectory)
        {
            if (ImGui::Button("<-"))
            {
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
            }
        }

        float padding = 16.0f;
        float thumbnailSize = 128.0f;
        float cellSize = thumbnailSize + padding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        columnCount = std::max(columnCount, 1);

        if (ImGui::BeginTable("AssetBrowserTable", columnCount))
        {
            for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
            {
                const auto& path = directoryEntry.path();
                bool isDirectory = directoryEntry.is_directory();
                if (!isDirectory && !AssetUtils::IsAssetExtensionSupported(path))
                    continue;
                
                auto relativePath = std::filesystem::relative(path, m_BaseDirectory);
                std::string filenameString = relativePath.filename().string();

                ImGui::TableNextColumn();
                ImGui::PushID(filenameString.c_str());

                auto icon = m_FileIcon;
                if (isDirectory)
                {
                    icon = m_DirectoryIcon;
                }
                else
                {
                    if (Engine::Get().GetAssetRegistry().Contains(path))
                    {
                        auto metadata = Engine::Get().GetAssetRegistry().Get(path);
                        if (metadata.Type == AssetType::Texture)
                        {
                            if (m_ThumbnailCache.contains(metadata.Handle))
                            {
                                icon = m_ThumbnailCache[metadata.Handle];
                            }
                            else
                            {
                                // TODO: This should not be blocking, instead do it async! Probably something for the AssetManager anyways.
                                auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(metadata.Handle);
                                if (texture)
                                {
                                    m_ThumbnailCache[metadata.Handle] = texture;
                                    icon = texture;
                                }
                            }
                        }
                    }
                }

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

                ImTextureID texID = (ImTextureID)0;
                if (icon && icon->GetHandle())
                {
                    texID = (ImTextureID)icon->GetTextureHandle().Get();
                }

                if (ImGui::ImageButton("btn", texID, { thumbnailSize, thumbnailSize }, { 0, 0 }, { 1, 1 }))
                {
                    if (isDirectory)
                    {
                        m_CurrentDirectory /= path.filename();
                    }
                }

                if (!isDirectory && ImGui::BeginDragDropSource())
                {
                    auto metadata = Engine::Get().GetAssetRegistry().Get(path);
                    uint64_t handleValue = (uint64_t)metadata.Handle;
             
                    ImGui::SetDragDropPayload(AssetUtils::GetDragDropPayload(metadata.Type), &handleValue, sizeof(uint64_t));
                    ImGui::Text(path.filename().string().c_str());
                    
                    ImGui::EndDragDropSource();
                }

                ImGui::PopStyleColor();

                ImGui::TextWrapped(filenameString.c_str());

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
}
