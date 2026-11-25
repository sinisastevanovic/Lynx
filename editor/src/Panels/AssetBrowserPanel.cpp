#include "AssetBrowserPanel.h"

#include <imgui.h>

#include <algorithm>

#include "../Utils/FontImporter.h"
#include "Lynx/Engine.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/Asset/Serialization/MaterialSerializer.h"
#include "Lynx/Asset/Serialization/SpriteSerializer.h"
#include "Lynx/Asset/Serialization/TerrainSerializer.h"
#include "Lynx/Asset/Terrain.h"
#include "../Utils/PlatformUtils.h"

namespace Lynx
{
    namespace Utils
    {
        std::filesystem::path GetNewAssetFilePath(const std::filesystem::path& basePath, const std::string& baseName, const std::string& extension)
        {
            std::filesystem::path newPath = basePath / (baseName + extension);

            int counter = 1;
            while (std::filesystem::exists(newPath))
            {
                newPath = basePath / (baseName + " (" + std::to_string(counter) + ")" + extension);
                counter++;
            }

            return newPath;
        }
    }
    
    AssetBrowserPanel::AssetBrowserPanel(const std::function<void(AssetHandle)>& selectionChangedCallback)
        : OnSelectedAssetChangedCallback(selectionChangedCallback),
          m_BaseDirectory(Engine::Get().GetContentPaths().ProjectAssets()),
          m_CurrentDirectory(m_BaseDirectory)
    {
        auto& assetManager = Engine::Get().GetAssetManager();
        m_DirectoryIcon = assetManager.GetEngineAsset<Texture>("Icons/DirectoryIcon_128px.png", AssetLoadMode::Blocking);
        m_FileIcon = assetManager.GetEngineAsset<Texture>("Icons/FileIcon_128px.png", AssetLoadMode::Blocking);
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
            if (ImGui::BeginPopupContextWindow("AssetBrowserPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem("Create Material"))
                {
                    auto mat = std::make_shared<Material>();
                    MaterialSerializer::Serialize(Utils::GetNewAssetFilePath(m_CurrentDirectory, "NewMaterial", ".lxmat"), mat);
                }
                if (ImGui::MenuItem("Create Sprite"))
                {
                    auto sprite = std::make_shared<Sprite>();
                    SpriteSerializer::Serialize(Utils::GetNewAssetFilePath(m_CurrentDirectory, "NewSprite", ".lxsprite"), sprite);
                }
                if (ImGui::MenuItem("Create Terrain..."))
                {
                    m_ShowTerrainModal = true;
                    m_TerrainError.clear();
                }

                ImGui::EndPopup();
            }
            
            for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
            {
                const auto& path = directoryEntry.path();
                bool isDirectory = directoryEntry.is_directory();

                // The folder a terrain keeps its tiles in belongs to the terrain, not to the user.
                if (isDirectory && path.extension() == ".tiles")
                    continue;
                bool isFontFile = path.extension() == ".ttf" || path.extension() == ".otf" || path.extension() == ".TTF" || path.extension() == ".OTF";
                if (!isDirectory && !AssetUtils::IsAssetExtensionSupported(path) && !isFontFile)
                    continue;
                
                auto relativePath = std::filesystem::relative(path, m_BaseDirectory);
                std::string filenameString = relativePath.filename().string();
                std::string filenameNoExtStr = relativePath.stem().string();

                ImGui::TableNextColumn();
                ImGui::PushID(filenameString.c_str());

                auto icon = m_FileIcon;
                glm::vec2 minUV = glm::vec2(0, 0);
                glm::vec2 maxUV = glm::vec2(1.0f, 1.0f);
                AssetMetadata metadata;
                if (isDirectory)
                {
                    icon = m_DirectoryIcon;
                }
                else
                {
                    if (Engine::Get().GetAssetRegistry().Contains(path))
                    {
                        metadata = Engine::Get().GetAssetRegistry().Get(path);
                        if (metadata.Type == AssetType::Texture || metadata.Type == AssetType::Sprite)
                        {
                            bool found = false;
                            if (m_ThumbnailCache.contains(metadata.Handle))
                            {
                                auto asset = Engine::Get().GetAssetManager().GetAsset(metadata.Handle);
                                const auto& thumbnail = m_ThumbnailCache[metadata.Handle];
                                if (thumbnail.Version == asset->GetVersion())
                                {
                                    icon = thumbnail.Texture;
                                    minUV = thumbnail.MinUV;
                                    maxUV = thumbnail.MaxUV;
                                    found = true;
                                }
                            }
                            if (!found)
                            {
                                // TODO: This should not be blocking, instead do it async! Probably something for the AssetManager anyways.
                                if (metadata.Type == AssetType::Texture)
                                {
                                    auto texture = Engine::Get().GetAssetManager().GetAsset<Texture>(metadata.Handle);
                                    if (texture)
                                    {
                                        if (texture->IsOnGpu())
                                        {
                                            ThumbnailData thumbnailData;
                                            thumbnailData.Texture = texture;
                                            thumbnailData.Version = texture->GetVersion();
                                            m_ThumbnailCache[metadata.Handle] = thumbnailData;
                                            icon = texture;
                                        }
                                    }
                                }
                                else
                                {
                                    auto sprite = Engine::Get().GetAssetManager().GetAsset<Sprite>(metadata.Handle);
                                    if (sprite)
                                    {
                                        if (sprite->GetTexture())
                                        {
                                            ThumbnailData thumbnailData;
                                            thumbnailData.Texture = sprite->GetTexture();
                                            thumbnailData.MinUV = sprite->GetUVMin();
                                            thumbnailData.MaxUV = sprite->GetUVMax();
                                            thumbnailData.Version = sprite->GetVersion();
                                            m_ThumbnailCache[metadata.Handle] = thumbnailData;
                                            icon = sprite->GetTexture();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                bool isSelected = path == m_SelectedPath;
                if (isSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                }


                ImTextureID texID = (ImTextureID)0;
                if (icon && icon->GetHandle() && icon->IsOnGpu())
                {
                    texID = static_cast<ImTextureID>(Engine::Get().GetRenderer().GetImGuiTexture(icon->GetTableIndex()));
                }
                else
                {
                    texID = static_cast<ImTextureID>(Engine::Get().GetRenderer().GetImGuiTexture(m_FileIcon->GetTableIndex()));
                }

                bool clicked = ImGui::ImageButton("btn", texID, { m_ThumbnailSize, m_ThumbnailSize }, { minUV.x, minUV.y }, { maxUV.x, maxUV.y });

                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    if (isDirectory)
                    {
                        m_CurrentDirectory /= path.filename();
                    }
                    else
                    {
                        OnSelectedAssetChangedCallback(metadata.Handle);
                    }
                    m_SelectedPath = path;
                }
                
                if (clicked)
                {
                    m_SelectedPath = path;
                }

                if (isSelected)
                {
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    // Draw a thick rounded border around the button
                    ImU32 selectionColor = ImGui::GetColorU32(ImGuiCol_ResizeGripHovered);
                    ImGui::GetWindowDrawList()->AddRect(min, max, selectionColor, 4.0f, 0, 2.0f);
                }
                
                if (!isDirectory && ImGui::BeginDragDropSource())
                {
                    auto metadata = Engine::Get().GetAssetRegistry().Get(path);
                    uint64_t handleValue = (uint64_t)metadata.Handle;
             
                    ImGui::SetDragDropPayload(AssetUtils::GetDragDropPayload(metadata.Type), &handleValue, sizeof(uint64_t));
                    ImGui::TextUnformatted(path.filename().string().c_str());
                    
                    ImGui::EndDragDropSource();
                }


                if (!isDirectory && ImGui::BeginPopupContextItem())
                {
                    if (isFontFile)
                    {
                        if (ImGui::MenuItem("Generate Font Atlas"))
                        {
                            m_FontImportPath = path;
                            m_ShowFontModal = true;
                        }
                        ImGui::Separator();
                    }
                    
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
                        m_IsRenaming = false;
                    }
                }
                else
                {
                    ImGui::TextWrapped("%s", filenameString.c_str());
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

        if (m_ShowTerrainModal)
        {
            ImGui::OpenPopup("Create Terrain");
            m_ShowTerrainModal = false;
        }

        if (ImGui::BeginPopupModal("Create Terrain", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Makes a terrain from a heightmap as terrain tools export it: a square of 16-bit heights, raw (.r16, .raw) or as a 16-bit grey PNG.");
            ImGui::Separator();

            ImGui::InputText("Heightmap", m_TerrainHeightmapPath, sizeof(m_TerrainHeightmapPath));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const std::string chosen = FileDialogs::OpenFile("Heightmap (*.r16;*.raw;*.png)\0*.r16;*.raw;*.png\0");
                if (!chosen.empty())
                    strcpy_s(m_TerrainHeightmapPath, chosen.c_str());
            }

            ImGui::InputFloat("Size (units a side)", &m_TerrainSize);
            ImGui::InputFloat("Lowest height", &m_TerrainLowest);
            ImGui::SetItemTooltip("The height that the smallest number of the heightmap stands for.");
            ImGui::InputFloat("Highest height", &m_TerrainHighest);
            ImGui::SetItemTooltip("The height that the largest number of the heightmap stands for.");

            if (!m_TerrainError.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_TerrainError.c_str());

            ImGui::Separator();
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                uint32_t resolution = 0;
                std::vector<uint16_t> heights;
                if (m_TerrainSize <= 0.0f || m_TerrainHighest <= m_TerrainLowest)
                {
                    m_TerrainError = "The size has to be above zero, and the highest height above the lowest.";
                }
                else if (!TerrainSerializer::ReadHeights(m_TerrainHeightmapPath, resolution, heights))
                {
                    m_TerrainError = "The heightmap could not be read. See the log.";
                }
                else
                {
                    // Made as it would be in code, then written out: its file is what the project keeps.
                    const Terrain terrain(resolution, m_TerrainSize, m_TerrainLowest, m_TerrainHighest, heights);
                    if (terrain.GetTilesPerSide() == 0)
                        m_TerrainError = "The heightmap is too small for a terrain: it needs at least 33 heights a side.";
                    else if (!terrain.Save(Utils::GetNewAssetFilePath(m_CurrentDirectory, "NewTerrain", ".lxterrain")))
                        m_TerrainError = "The terrain could not be written. See the log.";
                    else
                        ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (m_ShowFontModal)
        {
            ImGui::OpenPopup("Font Import Settings");
            m_ShowFontModal = false;
        }

        if (ImGui::BeginPopupModal("Font Import Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            std::string importStr = m_FontImportPath.filename().string();
            ImGui::Text("Import Settings for: %s", importStr.c_str());
            ImGui::Separator();

            ImGui::InputFloat("Pixel Range", &m_ImportPixelRange);
            ImGui::InputFloat("Font Scale", &m_ImportScale);

            ImGui::Separator();
            if (ImGui::Button("Generate", ImVec2(120, 0)))
            {
                FontImportOptions options;
                options.PixelRange = m_ImportPixelRange;
                options.Scale = m_ImportScale;

                FontImporter::ImportFont(m_FontImportPath, options);
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
        
        ImGui::End();
    }
}
