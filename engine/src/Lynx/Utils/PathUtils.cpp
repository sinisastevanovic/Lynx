#include "PathUtils.h"

namespace Lynx
{
    std::filesystem::path PathUtils::GetTmpPath()
    {
        if (!std::filesystem::exists("temp\\"))
            std::filesystem::create_directory("temp\\");
        return "temp\\";
    }
}
