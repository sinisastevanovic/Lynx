#include "AssetPropertiesPanel.h"

#include <imgui.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    void AssetPropertiesPanel::OnImGuiRender()
    {
        ImGui::Begin("Asset Properties");

        if (!m_SelectedAsset.IsValid())
        {
            ImGui::End();
            return;
        }

        auto asset = Engine::Get().GetAssetManager().GetAsset<Asset>(m_SelectedAsset);
        if (!asset)
        {
            ImGui::Text("Could not load asset!");
            ImGui::End();
            return;
        }

        if (asset->GetType() == AssetType::Texture)
        {
            DrawTextureProperties(std::static_pointer_cast<Texture>(asset));
        }
        else
        {
            ImGui::Text("Properties not available for this asset type.");
        }

        ImGui::End();
    }

    void AssetPropertiesPanel::DrawTextureProperties(std::shared_ptr<Texture> texture)
    {
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
            Engine::Get().GetAssetRegistry().UpdateMetadata(m_SelectedAsset, specPtr);
            Engine::Get().GetAssetManager().ReloadAsset(m_SelectedAsset);
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

    void AssetPropertiesPanel::OnSelectedAssetChanged(AssetHandle handle)
    {
        m_SelectedAsset = handle;
        m_IsDirty = false;

        if (!m_SelectedAsset.IsValid())
            return;

        auto asset = Engine::Get().GetAssetManager().GetAsset<Asset>(handle);
        if (asset && asset->GetType() == AssetType::Texture)
            m_EditingSpec = std::static_pointer_cast<Texture>(asset)->GetSpecification();
    }
}
