#include "ShaderUtils.h"

namespace Lynx
{
    std::vector<uint32_t> ShaderUtils::CompileGLSL(const std::string& source, shaderc_shader_kind kind, const char* fileName)
    {
        LX_CORE_TRACE("Compiling shader {0}", fileName);
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, fileName, options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            LX_CORE_ERROR("Shader Error: {0}", module.GetErrorMessage());
            return {};
        }

        LX_CORE_TRACE("Shader compiled successfully!");
        return {module.cbegin(), module.cend()};
    }
}
