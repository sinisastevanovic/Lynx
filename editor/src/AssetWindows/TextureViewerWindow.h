#pragma once
#include "AssetWindow.h"
#include "../Panels/TextureSettingsEditor.h"

namespace Lynx
{
    class Texture;

    // The window of a texture: the texture as it is on the GPU, after cooking. Any of its sizes, any
    // of its channels, zoomed and moved about, next to what it takes in memory and its settings.
    // Applying a setting cooks the texture anew, and the window shows the result.
    //
    // The wheel zooms, about the place under the mouse. Dragging with the left or middle button moves.
    class TextureViewerWindow : public AssetWindow
    {
    public:
        explicit TextureViewerWindow(AssetHandle texture);

    protected:
        const char* GetKindName() const override { return "Texture Viewer"; }
        bool DrawContents() override;

    private:
        void DrawControls(const Texture& texture);
        void DrawPicture(const Texture& texture);
        void DrawFacts(const Texture& texture);

    private:
        TextureSettingsEditor m_Settings;

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

        // Set at first and by "Fit": the next frame shows all of the texture.
        bool m_FitsNextFrame = true;
    };
}
