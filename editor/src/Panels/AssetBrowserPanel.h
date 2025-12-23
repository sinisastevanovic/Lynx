#pragma once
#include "Lynx/Asset/Asset.h"
#include "Lynx/Asset/Texture.h"

#include <filesystem>

namespace Lynx
{
    class AssetBrowserPanel
    {
    public:
        AssetBrowserPanel();
        virtual ~AssetBrowserPanel() = default;

        void OnImGuiRender();

    private:
        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;

        std::shared_ptr<Texture> m_DirectoryIcon;
        std::shared_ptr<Texture> m_FileIcon;

        // TODO: Setting to disable this!
        std::unordered_map<AssetHandle, std::shared_ptr<Texture>> m_ThumbnailCache;
    };
}

