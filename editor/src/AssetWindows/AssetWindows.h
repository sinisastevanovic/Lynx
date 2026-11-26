#pragma once
#include "AssetWindow.h"

#include "Lynx/Asset/AssetTypes.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Lynx
{
    // The asset windows that are open, and which kind of window each kind of asset opens.
    //
    // A new kind of window is one class derived from AssetWindow and one line where the editor
    // starts: windows.Register<MeshEditorWindow>(AssetType::StaticMesh).
    class AssetWindows
    {
    public:
        // Assets of this type open a window of this kind from now on. The kind is made with the asset's handle.
        template<typename Window>
        void Register(AssetType type)
        {
            m_Kinds[type] = [](AssetHandle asset) { return std::make_unique<Window>(asset); };
        }

        // True if assets of this type have a kind of window.
        bool CanOpen(AssetType type) const { return m_Kinds.contains(type); }

        // Opens the asset's window, or brings it to the front if it is open already.
        // Nothing happens for an asset whose type has no kind of window.
        void Open(AssetHandle asset);

        // Draws every open window and lets go of those that were closed. Called once a frame.
        void OnImGuiRender();

        // Closes all of them, when the editor shuts down or another project takes over.
        void CloseAll() { m_Windows.clear(); }

    private:
        std::unordered_map<AssetType, std::function<std::unique_ptr<AssetWindow>(AssetHandle)>> m_Kinds;
        std::vector<std::unique_ptr<AssetWindow>> m_Windows;
    };
}
