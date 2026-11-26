#pragma once

#include "TextureCooking.h"

#include <filesystem>
#include <optional>

namespace Lynx
{
    // Keeps cooked textures on disk, so a texture is decoded from its file and cooked only when the
    // file or its settings change. Loading a texture is then reading one file that holds every size
    // as the GPU takes it, with nothing to decode or scale.
    //
    // May be used from several threads at once: every texture has an entry of its own.
    class TextureCache
    {
    public:
        explicit TextureCache(const std::filesystem::path& directory);

        // A cooked texture, stored together with what it was made from: the file, told by its size
        // and the time it was last written, and the key of the settings it was cooked with
        // (TextureCookingKey). Loading returns it only if both still are what they were then.
        //
        // The file is told by its size and time, not by its content as the mesh cache tells a model:
        // a forest's textures are gigabytes, and reading them all to find that nothing changed would
        // take most of the time the cache saves.
        std::optional<CookedTexture> Load(const std::filesystem::path& sourceFile, uint64_t cookingKey) const;
        void Store(const std::filesystem::path& sourceFile, uint64_t cookingKey, const CookedTexture& texture) const;

    private:
        std::filesystem::path EntryPath(const std::filesystem::path& sourceFile) const;

        std::filesystem::path m_Directory;
    };
}
