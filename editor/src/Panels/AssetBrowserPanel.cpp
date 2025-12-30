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

        bool isAtRoot = (m_CurrentDirectory == m_BaseDirectory);
        ImGui::BeginDisabled(isAtRoot);
        if (ImGui::Button("<-"))
        {
            m_CurrentDirectory = m_CurrentDirectory.parent_path();
        }
        ImGui::EndDisabled();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f);
        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("##thumbnail_slider", &m_ThumbnailSize, 32.0f, 256.0f);
        ImGui::Separator();

        float padding = 16.0f;
        float cellSize = m_ThumbnailSize + padding;

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
                std::string filenameNoExtStr = relativePath.stem().string();

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
                                    if (texture->GetTextureHandle())
                                    {
                                        m_ThumbnailCache[metadata.Handle] = texture;
                                        icon = texture;
                                    }
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

                if (ImGui::ImageButton("btn", texID, { m_ThumbnailSize, m_ThumbnailSize }, { 0, 0 }, { 1, 1 }))
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

                if (!isDirectory && ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Rename"))
                    {
                        m_RenamingPath = path;
                        m_IsRenaming = true;
                        strcpy_s(m_RenameBuffer, filenameNoExtStr.c_str());
                    }

                    if (ImGui::MenuItem("Delete"))
                    {
                        m_DeletePath = path;
                        m_IsDeleting = true;
                    }
                    ImGui::EndPopup();
                }

                if (m_IsRenaming && m_RenamingPath == path)
                {
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        std::string newFileName = m_RenameBuffer + path.extension().string();
                        std::filesystem::path newPath = path.parent_path() / newFileName;
                        if (!m_RenameBuffer[0] || std::filesystem::exists(newPath))
                        {
                            LX_CORE_ERROR("Asset could not be renamed, file already exists!");
                        }
                        else
                        {
                            try
                            {
                                std::filesystem::rename(path, newPath);
                            }
                            catch (const std::exception& e)
                            {
                                LX_CORE_ERROR("Failed to rename file: {0}", e.what());
                            }
                        }
                        m_IsRenaming = false;
                    }

                    if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        m_IsDeleting = false;
                    }
                }
                else
                {
                    ImGui::TextWrapped(filenameString.c_str());
                }

                if (m_IsDeleting)
                {
                    ImGui::OpenPopup("Delete Asset?");
                    m_IsDeleting = false;
                }

                if (ImGui::BeginPopupModal("Delete Asset?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("Are you sure you want to delete this asset?\nThis action cannot be undone.\n\nFile: %s", m_DeletePath.filename().string().c_str());
                    ImGui::Separator();
                    if (ImGui::Button("Delete", ImVec2(120, 0)))
                    {
                        std::filesystem::remove_all(m_DeletePath);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
}
