#include "TextureViewerPanel.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Texture.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>

namespace Lynx
{
    namespace
    {
        // The settings and facts stand in a column this wide next to the picture.
        constexpr float SideWidth = 340.0f;

        // One notch of the wheel zooms by this much.
        constexpr float ZoomPerNotch = 1.2f;
        constexpr float LeastZoom = 1.0f / 64.0f;
        constexpr float MostZoom = 64.0f;
    }

    void TextureViewerPanel::OnSelectedAssetChanged(AssetHandle handle)
    {
        const bool isTexture = handle.IsValid() && Engine::Get().GetAssetRegistry().Get(handle).Type == AssetType::Texture;

        // Selecting something that is no texture leaves the viewer as it is.
        if (!isTexture)
            return;

        // The window opens for the texture and comes to the front, also if it shows that texture already.
        m_IsOpen = true;
        m_ComesToFront = true;
        if (handle == m_Handle)
            return;

        m_Handle = handle;
        m_Settings.SetTexture(handle);
        m_Size = 0;
        m_FitsNextFrame = true;
    }

    void TextureViewerPanel::OnImGuiRender()
    {
        // The viewer is a window that is opened for a texture and closed again, not a part of the layout.
        if (!m_IsOpen)
            return;

        const std::shared_ptr<Texture> texture = m_Handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Texture>(m_Handle) : nullptr;
        if (!texture)
        {
            // The texture was removed while the window showed it.
            m_IsOpen = false;
            return;
        }

        // It opens large enough to work in, and stays wherever it is put, docked or on its own.
        ImGui::SetNextWindowSize(ImVec2(1100.0f, 720.0f), ImGuiCond_FirstUseEver);
        if (std::exchange(m_ComesToFront, false))
            ImGui::SetNextWindowFocus();

        // The title names the texture. What follows the three hashes is what the layout knows the window by.
        const std::string title = std::filesystem::path(texture->GetFilePath()).filename().string() + " - Texture Viewer###TextureViewer";
        if (ImGui::Begin(title.c_str(), &m_IsOpen))
        {
            if (!texture->IsOnGpu())
            {
                ImGui::TextDisabled("Loading...");
            }
            else
            {
                DrawControls(*texture);

                const float pictureWidth = std::max(ImGui::GetContentRegionAvail().x - SideWidth - ImGui::GetStyle().ItemSpacing.x, 64.0f);
                ImGui::BeginChild("Picture", ImVec2(pictureWidth, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                DrawPicture(*texture);
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("Side", ImVec2(0.0f, 0.0f));
                DrawFacts(*texture);
                ImGui::Separator();
                m_Settings.Draw();
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void TextureViewerPanel::DrawControls(const Texture& texture)
    {
        ImGui::Checkbox("R", &m_ShowsRed);
        ImGui::SameLine();
        ImGui::Checkbox("G", &m_ShowsGreen);
        ImGui::SameLine();
        ImGui::Checkbox("B", &m_ShowsBlue);
        ImGui::SameLine();
        ImGui::Checkbox("A", &m_ShowsAlpha);
        ImGui::SetItemTooltip("With the alpha, the texture lies on grey squares that show through where it is see-through.\nThe alpha alone is shown as a grey.");

        // The sizes the texture is kept in, each half the one before.
        const int sizeCount = static_cast<int>(texture.GetSizeCount());
        m_Size = std::clamp(m_Size, 0, sizeCount - 1);
        if (sizeCount > 1)
        {
            ImGui::SameLine(0.0f, 24.0f);
            ImGui::SetNextItemWidth(220.0f);
            const uint32_t width = std::max(texture.GetWidth() >> m_Size, 1u);
            const uint32_t height = std::max(texture.GetHeight() >> m_Size, 1u);
            char label[64];
            std::snprintf(label, sizeof(label), "%u x %u", width, height);
            ImGui::SliderInt("Size", &m_Size, 0, sizeCount - 1, label);
            ImGui::SetItemTooltip("Which of its sizes is shown. Seen from far away, a texture is read from its smaller ones.");
        }

        ImGui::SameLine(0.0f, 24.0f);
        if (ImGui::Button("Fit"))
            m_FitsNextFrame = true;
        ImGui::SameLine();
        if (ImGui::Button("1:1"))
        {
            // One texel of the size that is shown on one pixel of the window.
            m_Zoom = 1.0f / static_cast<float>(1 << m_Size);
            m_Middle = { 0.5f, 0.5f };
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f %%", m_Zoom * 100.0f);
    }

    float TextureViewerPanel::ZoomToFit(const Texture& texture, glm::vec2 space) const
    {
        return std::clamp(std::min(space.x / static_cast<float>(texture.GetWidth()), space.y / static_cast<float>(texture.GetHeight())), LeastZoom, MostZoom);
    }

    void TextureViewerPanel::DrawPicture(const Texture& texture)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const glm::vec2 space(std::floor(available.x), std::floor(available.y));
        if (space.x < 1.0f || space.y < 1.0f)
            return;

        const glm::vec2 textureSize(static_cast<float>(texture.GetWidth()), static_cast<float>(texture.GetHeight()));
        if (m_FitsNextFrame)
        {
            m_Zoom = ZoomToFit(texture, space);
            m_Middle = { 0.5f, 0.5f };
            m_FitsNextFrame = false;
        }

        // The picture is a control of its own, so that the mouse over it is the viewer's.
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("Canvas", available, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
        const ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsItemActive())
            m_Middle -= glm::vec2(io.MouseDelta.x, io.MouseDelta.y) / (textureSize * m_Zoom);

        if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f)
        {
            // The place under the mouse stays where it is while the picture grows or shrinks around it.
            const glm::vec2 fromMiddle = glm::vec2(io.MousePos.x - start.x, io.MousePos.y - start.y) - space * 0.5f;
            const glm::vec2 underMouse = m_Middle + fromMiddle / (textureSize * m_Zoom);
            m_Zoom = std::clamp(m_Zoom * std::pow(ZoomPerNotch, io.MouseWheel), LeastZoom, MostZoom);
            m_Middle = underMouse - fromMiddle / (textureSize * m_Zoom);
        }
        m_Middle = glm::clamp(m_Middle, glm::vec2(0.0f), glm::vec2(1.0f));

        // The renderer draws the part of the texture that is seen into a picture as large as the space.
        Renderer& renderer = Engine::Get().GetRenderer();
        TexturePreview preview;
        preview.TextureIndex = texture.GetTableIndex();
        preview.Width = static_cast<uint32_t>(space.x);
        preview.Height = static_cast<uint32_t>(space.y);
        preview.UvSpan = space / (textureSize * m_Zoom);
        preview.UvStart = m_Middle - preview.UvSpan * 0.5f;
        preview.Size = static_cast<float>(m_Size);
        preview.ShowsRed = m_ShowsRed;
        preview.ShowsGreen = m_ShowsGreen;
        preview.ShowsBlue = m_ShowsBlue;
        preview.ShowsAlpha = m_ShowsAlpha;

        // A texel that is larger than a pixel is shown as the square it is.
        preview.BlendsBetweenTexels = m_Zoom * static_cast<float>(1 << m_Size) <= 1.0f;
        renderer.ShowTexturePreview(preview);

        if (const ImTextureID picture = static_cast<ImTextureID>(renderer.GetTexturePreviewImGuiTexture()))
            ImGui::GetWindowDrawList()->AddImage(picture, start, ImVec2(start.x + space.x, start.y + space.y));
    }

    void TextureViewerPanel::DrawFacts(const Texture& texture)
    {
        const TextureSpecification& settings = texture.GetSpecification();
        ImGui::TextWrapped("%s", std::filesystem::path(texture.GetFilePath()).filename().string().c_str());
        ImGui::Text("%u x %u, %u %s", texture.GetWidth(), texture.GetHeight(), texture.GetSizeCount(), texture.GetSizeCount() == 1 ? "size" : "sizes");
        ImGui::Text("Holds: %s", TextureContentNames[static_cast<size_t>(settings.Content)]);
        ImGui::Text("In video memory: %.2f MB", static_cast<double>(texture.GetByteCount()) / (1024.0 * 1024.0));
        ImGui::SetItemTooltip("All of its sizes together, as they lie on the GPU.");
    }
}
