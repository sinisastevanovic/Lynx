#pragma once
#include "Lynx/Asset/Asset.h"
#include "Lynx/Asset/TextureSpecification.h"

namespace Lynx
{
    // The settings of a texture as controls: what it holds, how it is compressed and read. Changes
    // are made to a copy, and reach the texture, its settings file and the GPU when they are applied.
    // The asset properties panel and the texture viewer each show one.
    class TextureSettingsEditor
    {
    public:
        // The texture whose settings are shown from now on. Changes that were not applied are dropped.
        void SetTexture(AssetHandle handle);

        // Draws the controls into the window that is being drawn. Nothing for a handle that is no texture.
        void Draw();

    private:
        AssetHandle m_Handle = AssetHandle::Null();
        TextureSpecification m_Editing;
        bool m_IsDirty = false;

        // The version of the texture the copy was taken from. The texture is loaded anew when another
        // window applies settings to it, and the copy is then taken again.
        uint32_t m_SeenVersion = 0;
    };
}
