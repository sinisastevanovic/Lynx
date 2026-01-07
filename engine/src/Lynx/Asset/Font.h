#pragma once
#include "Asset.h"
#include "Texture.h"

namespace Lynx
{
    struct FontGlyph
    {
        uint32_t Codepoint = 0;
        float Advance = 0.0f;

        float PlaneLeft = 0.0f, PlaneBottom = 0.0f, PlaneRight = 0.0f, PlaneTop = 0.0f;
        float AtlasLeft = 0.0f, AtlasBottom = 0.0f, AtlasRight = 0.0f, AtlasTop = 0.0f;
    };

    struct FontMetrics
    {
        float Ascender = 0.0f;
        float Descender = 0.0f;
        float LineHeight = 0.0f;
    };
    
    class LX_API Font : public Asset
    {
    public:
        Font(const std::string& filePath = "");
        ~Font() override = default;

        static AssetType GetStaticType() { return AssetType::Font; }
        AssetType GetType() const override { return GetStaticType(); }

        const FontGlyph* GetGlyph(uint32_t codepoint) const;
        const FontMetrics& GetMetrics() const { return m_Metrics; }

        float GetPixelRange() const { return m_PixelRange; }

        std::shared_ptr<Texture> GetTexture() const { return m_AtlasTexture; }
        void SetAtlasTexture(std::shared_ptr<Texture> atlasTexture) { m_AtlasTexture = atlasTexture; }

    protected:
        bool LoadSourceData() override;

    private:
        std::unordered_map<uint32_t, FontGlyph> m_Glyphs;
        FontMetrics m_Metrics;
        std::shared_ptr<Texture> m_AtlasTexture;
        float m_PixelRange = 3.0f;

        friend class FontImporter;
    };
}

