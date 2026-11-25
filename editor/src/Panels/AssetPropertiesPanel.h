#pragma once
#include "EditorPanel.h"
#include "Lynx/Asset/Material.h"
#include "Lynx/Asset/TextureSpecification.h"

#include <array>
#include <string>

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
        void DrawTerrainProperties();

    private:
        AssetHandle m_SelectedAssetHandle = AssetHandle::Null();
        std::shared_ptr<Asset> m_SelectedAsset;

        TextureSpecification m_EditingSpec;
        bool m_IsDirty = false;

        // A terrain's layers while they are edited: the material and the repeat of each, and a picture
        // of weights to read when the changes are applied.
        std::array<AssetHandle, 4> m_TerrainLayerMaterials;
        std::array<float, 4> m_TerrainLayerTileSizes{ 4.0f, 4.0f, 4.0f, 4.0f };
        char m_TerrainWeightsPath[512] = "";
        std::string m_TerrainError;
    };
}

