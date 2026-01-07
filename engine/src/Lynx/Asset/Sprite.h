#pragma once
#include "Asset.h"
#include "Lynx/UI/Core/UIGeometry.h"
#include <glm/glm.hpp>

namespace Lynx
{
    class Texture;
    
    class LX_API Sprite : public Asset
    {
    public:
        Sprite(const std::string& filePath = "");
        ~Sprite() override = default;

        std::shared_ptr<Texture> GetTexture() const { return m_Texture; }
        void SetTexture(std::shared_ptr<Texture> texture) { m_Texture = texture; }

        glm::vec2 GetUVMin() const { return m_UVMin; }
        glm::vec2 GetUVMax() const { return m_UVMax; }
        void SetUVs(glm::vec2 min, glm::vec2 max) { m_UVMin = min; m_UVMax = max; }

        UIThickness GetBorder() const { return m_Border; }
        void SetBorder(UIThickness border) { m_Border = border; }

        static AssetType GetAssetType() { return AssetType::Sprite; }
        virtual AssetType GetType() const override { return GetAssetType(); }

        bool LoadSourceData() override;
        bool Reload() override;
        bool DependsOn(AssetHandle handle) const override;

    private:
        std::shared_ptr<Texture> m_Texture;
        glm::vec2 m_UVMin = { 0.0f, 0.0f };
        glm::vec2 m_UVMax = { 1.0f, 1.0f };
        UIThickness m_Border;
        friend class AssetPropertiesPanel;
    };
}


