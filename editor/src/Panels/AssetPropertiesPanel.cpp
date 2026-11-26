#include "AssetPropertiesPanel.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/Asset/Serialization/MaterialSerializer.h"
#include "Lynx/Asset/Serialization/SpriteSerializer.h"
#include "Lynx/Asset/Serialization/TerrainSerializer.h"
#include "Lynx/Asset/Terrain.h"
#include "../Utils/PlatformUtils.h"
#include "Lynx/ImGui/LXUI.h"

namespace Lynx
{
    void AssetPropertiesPanel::OnImGuiRender()
    {
        ImGui::Begin("Asset Properties");

        if (!m_SelectedAsset)
        {
            ImGui::End();
            return;
        }

        switch (m_SelectedAsset->GetType())
        {
            case AssetType::Texture:
                DrawTextureProperties(); break;
            case AssetType::Material:
                DrawMaterialProperties(); break;
            case AssetType::Sprite:
                DrawSpriteProperties(); break;
            case AssetType::Terrain:
                DrawTerrainProperties(); break;
            default:
                ImGui::Text("Properties not available for this asset type.");
        }

        ImGui::End();
    }

    void AssetPropertiesPanel::DrawTerrainProperties()
    {
        auto terrain = std::static_pointer_cast<Terrain>(m_SelectedAsset);
        ImGui::Text("Terrain");
        ImGui::Separator();

        ImGui::Text("%u heights a side, %.0f units a side", terrain->GetResolution(), terrain->GetSize());
        ImGui::Text("%u by %u tiles of %u quads", terrain->GetTilesPerSide(), terrain->GetTilesPerSide(), terrain->GetTileQuads());
        ImGui::Text("Heights from %.1f to %.1f", terrain->GetLowest(), terrain->GetHighest());
        ImGui::Separator();

        ImGui::TextWrapped("Up to four kinds of ground. A layer without a material ends the list.");
        LXUI::BeginPropertyGrid();
        for (size_t layer = 0; layer < m_TerrainLayerMaterials.size(); ++layer)
        {
            const std::string number = std::to_string(layer + 1);
            if (LXUI::DrawAssetReference("Layer " + number + " Material", m_TerrainLayerMaterials[layer], { AssetType::Material }))
                m_IsDirty = true;
            if (LXUI::DrawDragFloat("Layer " + number + " Repeat (units)", m_TerrainLayerTileSizes[layer], 0.1f, 0.1f, 1000.0f, 4.0f))
                m_IsDirty = true;
        }
        LXUI::EndPropertyGrid();

        ImGui::Separator();
        ImGui::TextWrapped("The weights say how much of each layer shows where: a square picture, red for layer 1, green for 2, blue for 3, alpha for 4. Terrain tools export it as a splat map. Leave empty to keep the weights the terrain has.");
        if (ImGui::InputText("Weights", m_TerrainWeightsPath, sizeof(m_TerrainWeightsPath)))
            m_IsDirty = true;
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            const std::string chosen = FileDialogs::OpenFile("Picture (*.png;*.tga;*.jpg)\0*.png;*.tga;*.jpg\0");
            if (!chosen.empty())
            {
                strcpy_s(m_TerrainWeightsPath, chosen.c_str());
                m_IsDirty = true;
            }
        }

        if (!m_TerrainError.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_TerrainError.c_str());

        ImGui::Separator();
        ImGui::BeginDisabled(!m_IsDirty);
        if (ImGui::Button("Apply Changes"))
        {
            AssetManager& assets = Engine::Get().GetAssetManager();
            std::vector<TerrainLayer> layers;
            for (size_t layer = 0; layer < m_TerrainLayerMaterials.size(); ++layer)
            {
                std::shared_ptr<Material> material = m_TerrainLayerMaterials[layer].IsValid() ? assets.GetAsset<Material>(m_TerrainLayerMaterials[layer], AssetLoadMode::Blocking) : nullptr;
                if (!material)
                    break;
                layers.push_back({ std::move(material), m_TerrainLayerTileSizes[layer] });
            }

            m_TerrainError.clear();
            if (m_TerrainWeightsPath[0] != '\0' && !layers.empty())
            {
                uint32_t resolution = 0;
                std::vector<uint8_t> weights;
                if (!TerrainSerializer::ReadWeights(m_TerrainWeightsPath, resolution, weights))
                    m_TerrainError = "The weights could not be read. See the log.";
                else if (resolution % terrain->GetTilesPerSide() != 0)
                    m_TerrainError = "The weights' resolution has to be a whole number of times the terrain's " + std::to_string(terrain->GetTilesPerSide()) + " tiles a side.";
                else
                    terrain->SetLayers(layers, resolution, weights);
            }
            else
            {
                terrain->SetLayers(layers);
            }

            if (m_TerrainError.empty())
            {
                if (terrain->Save(terrain->GetFilePath()))
                {
                    m_TerrainWeightsPath[0] = '\0';
                    m_IsDirty = false;
                }
                else
                {
                    m_TerrainError = "The terrain could not be written. See the log.";
                }
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAssetHandle);
            OnSelectedAssetChanged(m_SelectedAssetHandle);
        }
    }

    void AssetPropertiesPanel::DrawTextureProperties()
    {
        auto texture = std::static_pointer_cast<Texture>(m_SelectedAsset);
        ImGui::Text("Texture: %s", m_EditingSpec.DebugName.c_str());
        ImGui::Separator();
        
        LXUI::BeginPropertyGrid();

        std::vector<std::string> formatStrings = { "None", "RGBA8", "RG16F", "RG32F", "R32I", "R8", "Depth32", "Depth24Stencil8" };
        int currFormat = (int)m_EditingSpec.Format;
        
        ImGui::Text("Format: %s", formatStrings[currFormat].c_str());

        // What the texture holds was guessed from the picture when it was first loaded. Undecided is
        // left out of the choices, and stands for a texture that has not been loaded yet.
        const std::vector<std::string> contentOptions(TextureContentNames.begin() + 1, TextureContentNames.end());
        int currentContent = std::max(static_cast<int>(m_EditingSpec.Content) - 1, 0);
        if (LXUI::DrawComboControl("Holds", currentContent, contentOptions))
        {
            m_EditingSpec.Content = static_cast<TextureContent>(currentContent + 1);
            m_IsDirty = true;
        }

        const std::vector<std::string> compressionOptions(TextureCompressionNames.begin(), TextureCompressionNames.end());
        int currentCompression = static_cast<int>(m_EditingSpec.Compression);
        if (LXUI::DrawComboControl("Compression", currentCompression, compressionOptions))
        {
            m_EditingSpec.Compression = static_cast<TextureCompression>(currentCompression);
            m_IsDirty = true;
        }

        if (LXUI::DrawCheckBox("Alpha Cuts Shapes Out", m_EditingSpec.KeepsCoverage)) m_IsDirty = true;
        if (m_EditingSpec.KeepsCoverage)
        {
            if (LXUI::DrawDragFloat("Cut-Off To Keep", m_EditingSpec.CoverageCutoff, 0.01f, 0.01f, 0.99f, 0.5f))
                m_IsDirty = true;
        }

        if (LXUI::DrawCheckBox("Generate Mips", m_EditingSpec.GenerateMips)) m_IsDirty = true;

        std::vector<std::string> filterOptions = { "Bilinear", "Nearest", "Trilinear" };
        int currentFilter = (int)m_EditingSpec.SamplerSettings.FilterMode;
        if (LXUI::DrawComboControl("Filter Mode", currentFilter, filterOptions))
        {
            m_EditingSpec.SamplerSettings.FilterMode = (TextureFilter)currentFilter;
            m_IsDirty = true;
        }

        std::vector<std::string> wrapOptions = { "Repeat", "Clamp", "Mirror" };
        int currentWrap = (int)m_EditingSpec.SamplerSettings.WrapMode;
        if (LXUI::DrawComboControl("Wrap Mode", currentWrap, wrapOptions))
        {
            m_EditingSpec.SamplerSettings.WrapMode = (TextureWrap)currentWrap;
            m_IsDirty = true;
        }

        if (m_EditingSpec.SamplerSettings.FilterMode == TextureFilter::Trilinear)
        {
            if (LXUI::DrawCheckBox("Use Anisotropy", m_EditingSpec.SamplerSettings.UseAnisotropy))
            {
                m_IsDirty = true;
            }
        }

        LXUI::EndPropertyGrid();
        
        ImGui::Spacing();
        ImGui::Separator();

        bool canApply = m_IsDirty || (m_EditingSpec != texture->GetSpecification());
        if (!canApply)
            ImGui::BeginDisabled();

        if (ImGui::Button("Apply Changes"))
        {
            texture->SetSpecification(m_EditingSpec);

            auto specPtr = std::make_shared<TextureSpecification>(m_EditingSpec);
            Engine::Get().GetAssetRegistry().UpdateMetadata(m_SelectedAssetHandle, specPtr);
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAssetHandle);
            //texture->Reload();
            m_IsDirty = false;
        }

        if (!canApply)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            m_EditingSpec = texture->GetSpecification();
            m_IsDirty = false;
        }
    }

    void AssetPropertiesPanel::DrawMaterialProperties()
    {
        auto material = std::static_pointer_cast<Material>(m_SelectedAsset);
        ImGui::Text("Material Editor");
        ImGui::Separator();
        
        LXUI::BeginPropertyGrid();

        std::vector<std::string> modes = { "Opaque", "Masked", "Translucent", "Additive" };
        int currentMode = (int)material->Mode;
        if (LXUI::DrawComboControl("Blend Mode", currentMode, modes))
        {
            material->Mode = (AlphaMode)currentMode;
            m_IsDirty = true;
        }

        if (LXUI::DrawCheckBox("Two Sided", material->TwoSided))
            m_IsDirty = true;

        if (LXUI::DrawColorControl("Albedo Color", material->AlbedoColor))
            m_IsDirty = true;

        if (LXUI::DrawAssetReference("Albedo Map", material->AlbedoTexture, { AssetType::Texture }))
            m_IsDirty = true;

        if (LXUI::DrawVec2Control("Tiling (Rows/Cols)", material->Tiling, 1.0f, 1.0f, 64.0f))
            m_IsDirty = true;

        if (LXUI::DrawDragFloat("Metallic", material->Metallic, 0.01f, 0.0f, 1.0f))
            m_IsDirty = true;
        if (LXUI::DrawDragFloat("Roughness", material->Roughness, 0.01f, 0.0f, 1.0f))
            m_IsDirty = true;
        if (LXUI::DrawAssetReference("Normal Map", material->NormalMap, {AssetType::Texture}))
            m_IsDirty = true;
        if (LXUI::DrawAssetReference("ORM Map", material->MetallicRoughnessTexture, {AssetType::Texture}))
            m_IsDirty = true;

        if (LXUI::DrawColor3Control("Emissive Color", material->EmissiveColor))
            m_IsDirty = true;
        if (LXUI::DrawDragFloat("Emissive Strength", material->EmissiveStrength, 0.1f, 0.0f, 100.0f))
            m_IsDirty = true;
        if (LXUI::DrawAssetReference("Emissive Map", material->EmissiveTexture, {AssetType::Texture}))
            m_IsDirty = true;
        
        LXUI::EndPropertyGrid();

        bool canApply = m_IsDirty;
        if (!canApply)
            ImGui::BeginDisabled();

        if (ImGui::Button("Apply Changes"))
        {
            MaterialSerializer::Serialize(material->GetFilePath(), material);
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAssetHandle);
            m_IsDirty = false;
        }

        if (!canApply)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAssetHandle);
            m_IsDirty = false;
        }
    }

    void AssetPropertiesPanel::DrawSpriteProperties()
    {
        auto sprite = std::static_pointer_cast<Sprite>(m_SelectedAsset);
        
        ImGui::Text("Sprite Editor");
        ImGui::Separator();
        
        LXUI::BeginPropertyGrid();

        AssetHandle texHandle = sprite->GetTexture() ? sprite->GetTexture()->GetHandle() : AssetHandle::Null();
        if (LXUI::DrawAssetReference("Texture", texHandle, {AssetType::Texture}))
        {
            if (texHandle.IsValid())
            {
                sprite->SetTexture(Engine::Get().GetAssetManager().GetAsset<Texture>(texHandle));
                m_IsDirty = true;
            }
        }

        if (LXUI::DrawVec2Control("UV Min", sprite->m_UVMin, 0.1f, 0.0f, 1.0f))
        {
            m_IsDirty = true;
        }

        if (LXUI::DrawVec2Control("UV Max", sprite->m_UVMax, 0.1f, 0.0f, 1.0f))
        {
            m_IsDirty = true;
        }

        UIThickness thickness = sprite->m_Border;
        if (LXUI::DrawUIThicknessControl("Borders", thickness))
        {
            sprite->m_Border = thickness;
            m_IsDirty = true;
        }
        
        LXUI::EndPropertyGrid();

        bool canApply = m_IsDirty;
        if (!canApply)
            ImGui::BeginDisabled();

        if (ImGui::Button("Apply Changes"))
        {
            SpriteSerializer::Serialize(sprite->GetFilePath(), sprite);
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAssetHandle);
            m_IsDirty = false;
        }

        if (!canApply)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAssetHandle);
            m_IsDirty = false;
        }
    }

    void AssetPropertiesPanel::OnSelectedAssetChanged(AssetHandle handle)
    {
        m_SelectedAssetHandle = handle;
        m_IsDirty = false;

        if (!m_SelectedAssetHandle.IsValid())
        {
            m_SelectedAsset = nullptr;
            return;
        }

        auto metadata = Engine::Get().GetAssetRegistry().Get(m_SelectedAssetHandle);
        if (metadata.Type == AssetType::Scene)
        {
            m_SelectedAsset = nullptr;
            return;
        }
        
        m_SelectedAsset = Engine::Get().GetAssetManager().GetAsset<Asset>(handle);
        if (m_SelectedAsset && m_SelectedAsset->GetType() == AssetType::Texture)
            m_EditingSpec = std::static_pointer_cast<Texture>(m_SelectedAsset)->GetSpecification();

        if (m_SelectedAsset && m_SelectedAsset->GetType() == AssetType::Terrain)
        {
            m_TerrainLayerMaterials.fill(AssetHandle::Null());
            m_TerrainLayerTileSizes.fill(4.0f);
            m_TerrainWeightsPath[0] = '\0';
            m_TerrainError.clear();

            const std::span<const TerrainLayer> layers = std::static_pointer_cast<Terrain>(m_SelectedAsset)->GetLayers();
            for (size_t layer = 0; layer < layers.size(); ++layer)
            {
                m_TerrainLayerMaterials[layer] = layers[layer].Material->GetHandle();
                m_TerrainLayerTileSizes[layer] = layers[layer].TileSize;
            }
        }
    }
}
