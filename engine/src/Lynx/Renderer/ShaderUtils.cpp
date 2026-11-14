#include "ShaderUtils.h"

#include "ShaderCache.h"
#include "Lynx/Engine.h"

namespace Lynx
{
    std::vector<uint32_t> ShaderUtils::CompileGLSL(const std::string& source, shaderc_shader_kind kind, const char* fileName)
    {
        const ShaderCache& cache = Engine::Get().GetShaderCache();
        const uint64_t cacheKey = ShaderCache::MakeKey(source, kind);

        std::vector<uint32_t> cachedSpirv = cache.Load(cacheKey);
        if (!cachedSpirv.empty())
            return cachedSpirv;

        LX_CORE_TRACE("Compiling shader {0}", fileName);

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        // Changing these settings requires bumping s_CompileSettingsVersion in ShaderCache.cpp.
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, fileName, options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            LX_CORE_ERROR("Shader Error: {0}", module.GetErrorMessage());
            return {};
        }

        std::vector<uint32_t> spirv(module.cbegin(), module.cend());
        cache.Store(cacheKey, spirv);
        return spirv;
    }
}
