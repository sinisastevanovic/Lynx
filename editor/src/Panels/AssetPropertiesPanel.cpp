#include "AssetPropertiesPanel.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/Asset/Serialization/MaterialSerializer.h"
#include "Lynx/Asset/Serialization/SpriteSerializer.h"
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
            default:
                ImGui::Text("Properties not available for this asset type.");
        }

        ImGui::End();
    }

    void AssetPropertiesPanel::DrawTextureProperties()
    {
        auto texture = std::static_pointer_cast<Texture>(m_SelectedAsset);
        ImGui::Text("Texture: %s", m_EditingSpec.DebugName.c_str());
        ImGui::Separator();
        
        LXUI::BeginPropertyGrid();

        std::vector<std::string> formatStrings = { "None", "RGBA8", "RG16F", "RG32F", "R32I", "R8", "Depth32", "Depth24Stencil8" };
        int currFormat = (int)m_EditingSpec.Format;
        
        ImGui::Text("Format: %s", formatStrings[currFormat]);
        if (LXUI::DrawCheckBox("Generate Mips", m_EditingSpec.GenerateMips)) m_IsDirty = true;
        if (LXUI::DrawCheckBox("Is sRGB", m_EditingSpec.IsSRGB)) m_IsDirty = true;

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
    }
}
