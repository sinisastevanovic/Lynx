#include "Shader.h"

#include "Lynx/Engine.h"
#include "Lynx/Renderer/Shader/ShaderCompiler.h"

namespace Lynx
{
    Shader::Shader(const std::string& filePath)
        : Asset(filePath)
    {
        LoadAndCompile();
    }

    bool Shader::Reload()
    {
        LoadAndCompile();
        return true;
    }

    void Shader::LoadAndCompile()
    {
        // The compiler reports what is wrong with a file. A file that does not compile leaves the program as it was.
        if (std::shared_ptr<const ShaderProgram> program = Engine::Get().GetShaderCompiler().Load(m_FilePath))
        {
            m_Program = std::move(program);
            IncrementVersion();
        }
    }
}
