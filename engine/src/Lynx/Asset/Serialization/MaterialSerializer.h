#pragma once
#include "Lynx/Asset/Material.h"
#include <filesystem>

namespace Lynx
{
    class LX_API MaterialSerializer
    {
    public:
        static bool Serialize(const std::filesystem::path& filePath, const std::shared_ptr<Material>& material);
        static bool Deserialize(const std::filesystem::path& filePath, Material& outMaterial);
    };

}
