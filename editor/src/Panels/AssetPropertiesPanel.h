#pragma once
#include "Lynx/Asset/TerrainScatter.h"
#include "EditorPanel.h"
#include "TextureSettingsEditor.h"
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
        void DrawTerrainScatterRules();

    private:
        AssetHandle m_SelectedAssetHandle = AssetHandle::Null();
        std::shared_ptr<Asset> m_SelectedAsset;

        TextureSettingsEditor m_TextureSettings;
        bool m_IsDirty = false;

        // A terrain's layers while they are edited: the material and the repeat of each, and a picture
        // of weights to read when the changes are applied.
        std::array<AssetHandle, 4> m_TerrainLayerMaterials;
        std::array<float, 4> m_TerrainLayerTileSizes{ 4.0f, 4.0f, 4.0f, 4.0f };
        char m_TerrainWeightsPath[512] = "";
        std::string m_TerrainError;

        // What the terrain is scattered with while it is edited: a rule and the handle of its mesh.
        struct ScatterRuleEdit
        {
            AssetHandle Mesh = AssetHandle::Null();
            ScatterRule Rule;
        };
        std::vector<ScatterRuleEdit> m_TerrainScatterRules;
    };
}

