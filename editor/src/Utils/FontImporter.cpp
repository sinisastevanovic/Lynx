#include "FontImporter.h"

#undef INFINITE
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdf-atlas-gen/FontGeometry.h>
#include <msdf-atlas-gen/BitmapAtlasStorage.h>

#include "Lynx/Asset/AssetManager.h"
#include "Lynx/Engine.h"
#include <nlohmann/json.hpp>
#include <fstream>

#include "Lynx/Utils/JsonHelpers.h"
#include "Lynx/Utils/PathUtils.h"

namespace Lynx
{
    using namespace msdf_atlas;

    struct AtlasData
    {
        std::vector<GlyphGeometry> Glyphs;
        FontGeometry Geometry;
    };
    
    void FontImporter::ImportFont(const std::filesystem::path& sourceFontPath, const FontImportOptions& options)
    {
        LX_CORE_INFO("Importing font: {0}", sourceFontPath.string());

        std::filesystem::path outputDir = sourceFontPath.parent_path();
        std::string fontName = sourceFontPath.stem().string();

        if (GenerateAtlas(sourceFontPath, outputDir, fontName, options))
        {
            LX_CORE_INFO("Font import successful!");
        }
        else
        {
            LX_CORE_ERROR("Font import failed!");
        }
    }

    bool FontImporter::GenerateAtlas(const std::filesystem::path& fontPath, const std::filesystem::path& outputDir, const std::string& fontName, const FontImportOptions& options)
    {
        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft)
            return false;

        std::string fontPathStr = fontPath.string();
        msdfgen::FontHandle* font = msdfgen::loadFont(ft, fontPathStr.c_str());
        if (!font)
        {
            msdfgen::deinitializeFreetype(ft);
            return false;
        }

        AtlasData data;
        Charset charset;

        // Add ASCII range + Latin-1 Supplement
        for (uint32_t c = 32; c <= 255; c++)
            charset.add(c);

        data.Geometry = FontGeometry(&data.Glyphs);
        int glyphsLoaded = data.Geometry.loadCharset(font, 1.0, charset);
        LX_CORE_TRACE("Loaded {0} glyphs", glyphsLoaded);
        const double maxCornerAngle = 3.0;
        for (GlyphGeometry& g : data.Glyphs)
        {
            g.edgeColoring(&msdfgen::edgeColoringSimple, maxCornerAngle, 0);
        }

        TightAtlasPacker packer;
        packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
        packer.setPixelRange(options.PixelRange);
        packer.setMiterLimit(1.0);
        packer.setInnerPixelPadding(0);
        packer.setOuterPixelPadding(0);
        packer.setMinimumScale(options.Scale);

        int remaining = packer.pack(data.Glyphs.data(), (int)data.Glyphs.size());
        if (remaining > 0)
        {
            LX_CORE_WARN("Could not pack {0} glyphs into atlas!", remaining);
        }

        int widthFinal, heightFinal;
        packer.getDimensions(widthFinal, heightFinal);
        ImmediateAtlasGenerator<float, 3, msdfGenerator, BitmapAtlasStorage<byte, 3>> generator(widthFinal, heightFinal);

        GeneratorAttributes attributes;
        generator.setAttributes(attributes);
        generator.setThreadCount(4);
        generator.generate(data.Glyphs.data(), (int)data.Glyphs.size());

        msdfgen::BitmapConstRef<byte, 3> bitmap = generator.atlasStorage();
        std::filesystem::path texturePath = outputDir / (fontName + ".png");
        std::string tempPath = (PathUtils::GetTmpPath() / (fontName + ".png")).string();

        msdfgen::savePng(bitmap, tempPath.c_str());

        std::shared_ptr<TextureSpecification> specification = std::make_shared<TextureSpecification>();
        specification->GenerateMips = false;
        specification->Format = TextureFormat::RGBA8; // TODO: Add RGB8!
        specification->IsSRGB = false;
        specification->SamplerSettings = { TextureWrap::Clamp, TextureFilter::Bilinear };
        AssetHandle textureAtlasHandle = Engine::Get().GetAssetRegistry().ImportAssetFromTemp(tempPath, texturePath, specification);

        nlohmann::json fontData;
        auto& metrics = data.Geometry.getMetrics();
        fontData["Metrics"] = {
            { "LineHeight", metrics.lineHeight },
            { "Ascender", metrics.ascenderY },
            { "Descender", metrics.descenderY }
        };
        fontData["AtlasTexture"] = textureAtlasHandle;
        fontData["PixelRange"] = options.PixelRange;

        for (const auto& glyph : data.Glyphs)
        {
            double al, ab, ar, at;
            glyph.getQuadAtlasBounds(al, ab, ar, at);

            double u0 = al / widthFinal;
            double v0 = (heightFinal - at) / heightFinal;
            double u1 = ar / widthFinal;
            double v1 = (heightFinal - ab) / heightFinal;

            double pl, pb, pr, pt;
            glyph.getQuadPlaneBounds(pl, pb, pr, pt);

            nlohmann::json glyphJson;
            glyphJson["Codepoint"] = glyph.getCodepoint();
            glyphJson["Advance"] = glyph.getAdvance();

            glyphJson["PlaneBounds"] = { {"L", pl}, {"B", pb}, {"R", pr}, {"T", pt} };
            glyphJson["AtlasBounds"] = { {"L", u0}, {"B", v1}, {"R", u1}, {"T", v0} };

            fontData["Glyphs"].push_back(glyphJson);
        }

        std::filesystem::path fontAssetPath = outputDir / (fontName + ".lxfont");
        std::ofstream file(fontAssetPath);
        file << fontData.dump(4);

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return true;
    }
}
