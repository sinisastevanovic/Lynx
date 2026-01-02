#pragma once
#include "Lynx/Asset/Asset.h"
#include "Lynx/Asset/Texture.h"

#include <filesystem>

#include "EditorPanel.h"

namespace Lynx
{
    class AssetBrowserPanel : public EditorPanel
    {
    public:
        AssetBrowserPanel(const std::function<void(AssetHandle)>& selectionChangedCallback);
        virtual ~AssetBrowserPanel() = default;

        virtual void OnImGuiRender() override;

    private:
        std::function<void(AssetHandle)> OnSelectedAssetChangedCallback = nullptr;
        std::filesystem::path m_SelectedPath;
        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;

        std::shared_ptr<Texture> m_DirectoryIcon;
        std::shared_ptr<Texture> m_FileIcon;

        // TODO: Setting to disable this!
        std::unordered_map<AssetHandle, std::shared_ptr<Texture>> m_ThumbnailCache;

        float m_ThumbnailSize = 128.0f;

        std::filesystem::path m_RenamingPath;
        char m_RenameBuffer[256] = "";
        bool m_IsRenaming = false;
        
        std::filesystem::path m_DeletePath;
        bool m_IsDeleting = false;
    };
}

