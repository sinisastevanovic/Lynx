#pragma once
#include "Lynx/Asset/Sprite.h"
#include <filesystem>

namespace Lynx
{
    class LX_API SpriteSerializer
    {
    public:
        static bool Serialize(const std::filesystem::path& filePath, const std::shared_ptr<Sprite>& sprite);
        static bool Deserialize(const std::filesystem::path& filePath, Sprite& putSprite);
    };

}
