#pragma once

#include "Lynx/Core.h"
#include <filesystem>

namespace Lynx
{
    class LX_API PathUtils
    {
    public:
        static std::filesystem::path GetTmpPath();
    };
}
