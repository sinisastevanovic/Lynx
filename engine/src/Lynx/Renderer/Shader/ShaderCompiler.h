#pragma once

#include "ShaderProgram.h"

#include <filesystem>
#include <memory>
#include <mutex>

namespace Lynx
{
    class ShaderCache;

    // Turns .slang files into shader programs.
    //
    // A program is compiled only when its file, or a file it imports, has changed since it was last
    // compiled. Otherwise it comes from the shader cache on disk, and the Slang compiler, which takes
    // a noticeable time to start, is not even loaded.
    class ShaderCompiler
    {
    public:
        // Modules that shaders import by name are looked for in moduleDirectory.
        ShaderCompiler(const ShaderCache& cache, std::filesystem::path moduleDirectory);
        ~ShaderCompiler();

        // Returns null, and logs what the compiler reported, if the file does not compile.
        // May be called from any thread.
        std::shared_ptr<const ShaderProgram> Load(const std::filesystem::path& file);

    private:
        struct SlangSession;

        const ShaderCache& m_Cache;
        std::filesystem::path m_ModuleDirectory;

        // Created on the first compilation.
        std::unique_ptr<SlangSession> m_Slang;
        std::mutex m_Mutex;
    };
}
