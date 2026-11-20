#pragma once

#include <cstdint>
#include <vector>

namespace Lynx
{
    // A picture of a finished frame, read back from the GPU.
    struct Screenshot
    {
        uint64_t Tag = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Rgba; // Four bytes per pixel, rows from top to bottom.
    };
}
