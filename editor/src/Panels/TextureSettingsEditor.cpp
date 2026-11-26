#include "TextureSettingsEditor.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Texture.h"
#include "Lynx/ImGui/LXUI.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Lynx
{
    void TextureSettingsEditor::SetTexture(AssetHandle handle)
    {
        m_Handle = handle;
        m_IsDirty = false;
        m_SeenVersion = 0;

        if (const std::shared_ptr<Texture> texture = handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Texture>(handle) : nullptr)
        {
            m_Editing = texture->GetSpecification();
            m_SeenVersion = texture->GetVersion();
        }
    }

    void TextureSettingsEditor::Draw()
    {
        const std::shared_ptr<Texture> texture = m_Handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Texture>(m_Handle) : nullptr;
        if (!texture)
            return;

        // Settings applied elsewhere, or a guess made when the texture finished loading, are taken
        // over unless something here is waiting to be applied.
        if (!m_IsDirty && texture->GetVersion() != m_SeenVersion)
        {
            m_Editing = texture->GetSpecification();
            m_SeenVersion = texture->GetVersion();
        }

        // The scope keeps the controls apart from those of another window's editor.
        ImGui::PushID(this);
        LXUI::BeginPropertyGrid();

        // What the texture holds was guessed from the picture when it was first loaded. Undecided is
        // left out of the choices, and stands for a texture that has not been loaded yet.
        const std::vector<std::string> contentOptions(TextureContentNames.begin() + 1, TextureContentNames.end());
        int currentContent = std::max(static_cast<int>(m_Editing.Content) - 1, 0);
        if (LXUI::DrawComboControl("Holds", currentContent, contentOptions))
        {
            m_Editing.Content = static_cast<TextureContent>(currentContent + 1);
            m_IsDirty = true;
        }

        const std::vector<std::string> compressionOptions(TextureCompressionNames.begin(), TextureCompressionNames.end());
        int currentCompression = static_cast<int>(m_Editing.Compression);
        if (LXUI::DrawComboControl("Compression", currentCompression, compressionOptions))
        {
            m_Editing.Compression = static_cast<TextureCompression>(currentCompression);
            m_IsDirty = true;
        }

        if (LXUI::DrawCheckBox("Alpha Cuts Shapes Out", m_Editing.KeepsCoverage))
            m_IsDirty = true;
        if (LXUI::DrawCheckBox("Generate Mips", m_Editing.GenerateMips))
            m_IsDirty = true;

        const std::vector<std::string> filterOptions = { "Bilinear", "Nearest", "Trilinear" };
        int currentFilter = static_cast<int>(m_Editing.SamplerSettings.FilterMode);
        if (LXUI::DrawComboControl("Filter Mode", currentFilter, filterOptions))
        {
            m_Editing.SamplerSettings.FilterMode = static_cast<TextureFilter>(currentFilter);
            m_IsDirty = true;
        }

        const std::vector<std::string> wrapOptions = { "Repeat", "Clamp", "Mirror" };
        int currentWrap = static_cast<int>(m_Editing.SamplerSettings.WrapMode);
        if (LXUI::DrawComboControl("Wrap Mode", currentWrap, wrapOptions))
        {
            m_Editing.SamplerSettings.WrapMode = static_cast<TextureWrap>(currentWrap);
            m_IsDirty = true;
        }

        if (m_Editing.SamplerSettings.FilterMode == TextureFilter::Trilinear)
        {
            if (LXUI::DrawCheckBox("Use Anisotropy", m_Editing.SamplerSettings.UseAnisotropy))
                m_IsDirty = true;
        }

        LXUI::EndPropertyGrid();
        ImGui::Spacing();

        ImGui::BeginDisabled(!m_IsDirty);
        if (ImGui::Button("Apply Changes"))
        {
            // The texture is cooked and loaded anew with the settings, which are saved next to its file.
            // They are a choice now, which no guess and nothing that uses the texture changes again.
            m_Editing.Origin = TextureSettingsOrigin::Chosen;
            texture->SetSpecification(m_Editing);
            Engine::Get().GetAssetRegistry().UpdateMetadata(m_Handle, std::make_shared<TextureSpecification>(m_Editing));
            Engine::Get().GetAssetManager().ReloadAsset(m_Handle);
            m_IsDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            m_Editing = texture->GetSpecification();
            m_IsDirty = false;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}
