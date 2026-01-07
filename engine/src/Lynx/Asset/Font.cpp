#include "Font.h"

#include "Lynx/Engine.h"

namespace Lynx
{
    Font::Font(const std::string& filePath)
        : Asset(filePath)
    {
    }

    const FontGlyph* Font::GetGlyph(uint32_t codepoint) const
    {
        auto it = m_Glyphs.find(codepoint);
        if (it != m_Glyphs.end())
            return &it->second;
        return nullptr;
    }

    bool Font::LoadSourceData()
    {
        if (m_FilePath.empty())
            return true;
        
        std::ifstream stream(m_FilePath);
        if (!stream.is_open())
        {
            LX_CORE_ERROR("Failed to open font asset file: {0}", m_FilePath);
            return false;
        }

        nlohmann::json data;
        try
        {
            stream >> data;
        }
        catch (const std::exception& e)
        {
            LX_CORE_ERROR("Failed to parse font file: {0}", m_FilePath);
            return false;
        }

        m_Metrics.LineHeight = data["Metrics"].value("LineHeight", 1.0f);
        m_Metrics.Ascender = data["Metrics"].value("Ascender", 1.0f);
        m_Metrics.Descender = data["Metrics"].value("Descender", -0.2f);

        AssetHandle textureHandle = data["AtlasTexture"].get<AssetHandle>();
        if (textureHandle.IsValid())
        {
            m_AtlasTexture = Engine::Get().GetAssetManager().GetAsset<Texture>(textureHandle);
        }

        m_PixelRange = data.value("PixelRange", 3.0f);

        if (data.contains("Glyphs"))
        {
            for (const auto& glyphData : data["Glyphs"])
            {
                FontGlyph glyph;
                glyph.Codepoint = glyphData.value("Codepoint", 0u);
                glyph.Advance = glyphData.value("Advance", 0.0f);

                auto& pb = glyphData["PlaneBounds"];
                glyph.PlaneLeft = pb.value("L", 0.0f);
                glyph.PlaneBottom = pb.value("B", 0.0f);
                glyph.PlaneRight = pb.value("R", 0.0f);
                glyph.PlaneTop = pb.value("T", 0.0f);

                auto& ab = glyphData["AtlasBounds"];
                glyph.AtlasLeft = ab.value("L", 0.0f);
                glyph.AtlasBottom = ab.value("B", 0.0f);
                glyph.AtlasRight = ab.value("R", 0.0f);
                glyph.AtlasTop = ab.value("T", 0.0f);

                m_Glyphs[glyph.Codepoint] = glyph;
            }
        }

        return true;
    }
}
