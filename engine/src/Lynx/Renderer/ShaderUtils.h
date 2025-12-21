#pragma once

#include "Lynx/Core.h"
#include <shaderc/shaderc.hpp>

namespace Lynx
{
    class ShaderUtils
    {
    public:
        static std::vector<uint32_t> CompileGLSL(const std::string& source, shaderc_shader_kind kind, const char* fileName);
    
    };
}

