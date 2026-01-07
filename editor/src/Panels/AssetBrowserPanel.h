#pragma once
#include "Lynx/Asset/Asset.h"
#include "Lynx/Asset/Texture.h"

#include <filesystem>
#include <glm/vec2.hpp>

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

        struct ThumbnailData
        {
            std::shared_ptr<Texture> Texture = nullptr;
            glm::vec2 MinUV = glm::vec2(0.0f);
            glm::vec2 MaxUV = glm::vec2(1.0f);
            uint32_t Version = 0;
        };
        // TODO: Setting to disable this!
        std::unordered_map<AssetHandle, ThumbnailData> m_ThumbnailCache;

        float m_ThumbnailSize = 128.0f;

        std::filesystem::path m_RenamingPath;
        char m_RenameBuffer[256] = "";
        bool m_IsRenaming = false;
        
        std::filesystem::path m_DeletePath;
        bool m_IsDeleting = false;

        bool m_ShowFontModal = false;
        std::filesystem::path m_FontImportPath;
        float m_ImportPixelRange = 3.0f;
        float m_ImportScale = 48.0f;
    };
}

