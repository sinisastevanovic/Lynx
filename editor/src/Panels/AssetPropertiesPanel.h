#pragma once
#include "EditorPanel.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/TextureSpecification.h"

namespace Lynx
{
    class Texture;
    
    class AssetPropertiesPanel : public EditorPanel
    {
    public:
        AssetPropertiesPanel() = default;

        virtual void OnImGuiRender() override;
        virtual void OnSelectedAssetChanged(AssetHandle handle) override;

    private:
        void DrawTextureProperties();
        void DrawMaterialProperties();
        void DrawSpriteProperties();

    private:
        AssetHandle m_SelectedAssetHandle = AssetHandle::Null();
        std::shared_ptr<Asset> m_SelectedAsset;

        TextureSpecification m_EditingSpec;
        bool m_IsDirty = false;
    };
}

