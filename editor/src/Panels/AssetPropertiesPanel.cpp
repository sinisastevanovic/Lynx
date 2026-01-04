#include "AssetPropertiesPanel.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Serialization/MaterialSerializer.h"
#include "Lynx/ImGui/EditorUIHelpers.h"

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

        if (m_SelectedAsset->GetType() == AssetType::Texture)
        {
            DrawTextureProperties();
        }
        else if (m_SelectedAsset->GetType() == AssetType::Material)
        {
            DrawMaterialProperties();
        }
        else
        {
            ImGui::Text("Properties not available for this asset type.");
        }

        ImGui::End();
    }

    void AssetPropertiesPanel::DrawTextureProperties()
    {
        auto texture = std::static_pointer_cast<Texture>(m_SelectedAsset);
        ImGui::Text("Texture: %s", m_EditingSpec.DebugName.c_str());
        ImGui::Separator();

        if (ImGui::Checkbox("Generate Mips", &m_EditingSpec.GenerateMips)) m_IsDirty = true;
        if (ImGui::Checkbox("Is sRGB", &m_EditingSpec.IsSRGB)) m_IsDirty = true;

        const char* filterOptions[] = { "Bilinear", "Nearest", "Trilinear" };
        int currentFilter = (int)m_EditingSpec.SamplerSettings.FilterMode;
        if (ImGui::Combo("Filter Mode", &currentFilter, filterOptions, IM_ARRAYSIZE(filterOptions)))
        {
            m_EditingSpec.SamplerSettings.FilterMode = (TextureFilter)currentFilter;
            m_IsDirty = true;
        }

        const char* wrapOptions[] = { "Repeat", "Clamp", "Mirror" };
        int currentWrap = (int)m_EditingSpec.SamplerSettings.WrapMode;
        if (ImGui::Combo("Wrap Mode", &currentWrap, wrapOptions, IM_ARRAYSIZE(wrapOptions)))
        {
            m_EditingSpec.SamplerSettings.WrapMode = (TextureWrap)currentWrap;
            m_IsDirty = true;
        }

        if (m_EditingSpec.SamplerSettings.FilterMode == TextureFilter::Trilinear)
        {
            if (ImGui::Checkbox("Use Anisotropy", &m_EditingSpec.SamplerSettings.UseAnisotropy))
            {
                m_IsDirty = true;
            }
        }

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

        const char* modes[] = { "Opaque", "Masked", "Translucent", "Additive" };
        int currentMode = (int)material->Mode;
        if (ImGui::Combo("Blend Mode", &currentMode, modes, 4))
        {
            material->Mode = (AlphaMode)currentMode;
            m_IsDirty = true;
        }

        if (ImGui::ColorEdit4("Albedo Color", &material->AlbedoColor.r))
            m_IsDirty = true;

        if (EditorUIHelpers::DrawAssetSelection("Albedo Map", material->AlbedoTexture, AssetType::Texture))
            m_IsDirty = true;

        if (ImGui::DragFloat("Metallic", &material->Metallic, 0.01f, 0.0f, 1.0f))
            m_IsDirty = true;
        if (ImGui::DragFloat("Roughness", &material->Roughness, 0.01f, 0.0f, 1.0f))
            m_IsDirty = true;
        if (EditorUIHelpers::DrawAssetSelection("Normal Map", material->NormalMap, AssetType::Texture))
            m_IsDirty = true;
        if (EditorUIHelpers::DrawAssetSelection("ORM Map", material->MetallicRoughnessTexture, AssetType::Texture))
            m_IsDirty = true;

        if (ImGui::ColorEdit3("Emissive Color", &material->EmissiveColor.r))
            m_IsDirty = true;
        if (ImGui::DragFloat("Emissive Strength", &material->EmissiveStrength, 0.1f, 0.0f, 100.0f))
            m_IsDirty = true;
        if (EditorUIHelpers::DrawAssetSelection("Emissive Map", material->EmissiveTexture, AssetType::Texture))
            m_IsDirty = true;

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
