#pragma once
#include "EditorPanel.h"
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
        void DrawTextureProperties(std::shared_ptr<Texture> texture);

    private:
        AssetHandle m_SelectedAsset = AssetHandle::Null();

        TextureSpecification m_EditingSpec;
        bool m_IsDirty = false;
    };
}

