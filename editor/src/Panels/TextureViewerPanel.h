#pragma once
#include "EditorPanel.h"
#include "TextureSettingsEditor.h"

#include <glm/glm.hpp>

namespace Lynx
{
    class Texture;

    // A window that shows a texture as it is on the GPU, after cooking: any of its sizes, any of
    // its channels, zoomed and moved about, next to what it takes in memory and its settings. Applying
    // a setting cooks the texture anew, and the window shows the result.
    //
    // It opens when a texture is picked in the asset browser with a double click, and is closed like
    // any window. There is one: picking another texture shows that one in it.
    //
    // The wheel zooms, about the place under the mouse. Dragging with the left or middle button moves.
    class TextureViewerPanel : public EditorPanel
    {
    public:
        void OnImGuiRender() override;
        void OnSelectedAssetChanged(AssetHandle handle) override;

    private:
        void DrawControls(const Texture& texture);
        void DrawPicture(const Texture& texture);
        void DrawFacts(const Texture& texture);

        // How many pixels of the window one texel of the largest size takes if all of the texture is to fit.
        float ZoomToFit(const Texture& texture, glm::vec2 space) const;

    private:
        AssetHandle m_Handle = AssetHandle::Null();
        TextureSettingsEditor m_Settings;

        // Closed until a texture is picked. And set when one is, so that the window comes to the front.
        bool m_IsOpen = false;
        bool m_ComesToFront = false;

        // Which size is shown, 0 for the largest, and which channels.
        int m_Size = 0;
        bool m_ShowsRed = true;
        bool m_ShowsGreen = true;
        bool m_ShowsBlue = true;
        bool m_ShowsAlpha = false;

        // How many pixels of the window one texel of the largest size takes, and the place of the
        // texture that lies in the middle of the picture, in texture coordinates.
        float m_Zoom = 1.0f;
        glm::vec2 m_Middle{ 0.5f, 0.5f };

        // Set when another texture is selected: the next frame shows all of it.
        bool m_FitsNextFrame = true;
    };
}
