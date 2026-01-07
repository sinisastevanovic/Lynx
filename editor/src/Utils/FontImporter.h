#pragma once
#include <filesystem>

namespace Lynx
{
    struct FontImportOptions
    {
        float PixelRange = 3.0f;
        float Scale = 48.0f;
    };
    
    class FontImporter
    {
    public:
        static void ImportFont(const std::filesystem::path& sourceFontPath, const FontImportOptions& options);

    private:
        static bool GenerateAtlas(const std::filesystem::path& fontPath,
                                  const std::filesystem::path& outputDir,
                                  const std::string& fontName, const FontImportOptions& options);
    
    };
}

