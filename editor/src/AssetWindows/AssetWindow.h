#pragma once
#include "Lynx/Asset/Asset.h"

#include <glm/glm.hpp>

#include <string>

namespace Lynx
{
    // A window of its own for one asset: what is opened by a double click on the asset, to look at
    // it or to work on it, and closed when that is done. A texture's viewer is one. An editor for a
    // mesh or a material would be the next.
    //
    // It is a window of the operating system, with its title bar, its place in the task bar and its
    // close button, apart from the editor's main window. Several can be open at once, one per asset.
    // AssetWindows keeps them and opens the right kind for an asset.
    //
    // A kind of window derives from this, says what it is called and draws what it shows.
    class AssetWindow
    {
    public:
        explicit AssetWindow(AssetHandle asset);
        virtual ~AssetWindow() = default;

        AssetHandle GetAsset() const { return m_Asset; }

        // False once the window was closed, or its asset is gone. AssetWindows then lets go of it.
        bool IsOpen() const { return m_IsOpen; }

        // The next frame puts the window in front of the others and gives it the keyboard.
        void ComeToFront() { m_ComesToFront = true; }

        // Draws the window. Called once a frame while it is open.
        void OnImGuiRender();

    protected:
        // What the kind of window is called, shown after the asset's name in the title: "Texture Viewer".
        virtual const char* GetKindName() const = 0;

        // The size the window opens in the first time, in pixels. It then keeps the size it is given.
        virtual glm::vec2 GetFirstSize() const { return { 1100.0f, 720.0f }; }

        // Draws what the window shows. Returns false if there is nothing to show any more, because
        // the asset was removed: the window then closes.
        virtual bool DrawContents() = 0;

    private:
        AssetHandle m_Asset;
        bool m_IsOpen = true;
        bool m_ComesToFront = true;
    };
}
