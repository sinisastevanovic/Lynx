#pragma once

#include "Lynx/Core.h"
#include <shaderc/shaderc.hpp>

namespace Lynx
{
    class ShaderUtils
    {
    public:
        // Returns SPIR-V for the shader source, from the shader cache when the same source was compiled before.
        // Returns an empty vector, and logs the compiler output, if the source does not compile.
        static std::vector<uint32_t> CompileGLSL(const std::string& source, shaderc_shader_kind kind, const char* fileName);
    
    };
}

