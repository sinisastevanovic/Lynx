#include "Shader.h"

#include <shaderc/shaderc.h>

#include "Lynx/Engine.h"
#include "Lynx/Renderer/ShaderUtils.h"

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
        Engine::Get().GetRenderer().ReloadShaders();
        return true;
    }

    void Shader::LoadAndCompile()
    {
        std::ifstream in(m_FilePath, std::ios::in | std::ios::binary);
        if (in)
        {
            std::string result;
            in.seekg(0, std::ios::end);
            result.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&result[0], result.size());
            in.close();

            auto sources = PreProcess(result);

            auto device = Engine::Get().GetRenderer().GetDeviceHandle();

            for (auto& [type, src] : sources)
            {
                shaderc_shader_kind kind;
                if (type == nvrhi::ShaderType::Vertex) kind = shaderc_vertex_shader;
                else if (type == nvrhi::ShaderType::Pixel) kind = shaderc_fragment_shader;
                else if (type == nvrhi::ShaderType::Compute) kind = shaderc_compute_shader;
                else continue;
                
                std::vector<uint32_t> spirv = ShaderUtils::CompileGLSL(src, kind, m_FilePath.c_str());
                if (spirv.empty())
                    continue;

                nvrhi::ShaderDesc desc(type);
                auto handle = device->createShader(desc, spirv.data(), spirv.size() * 4);

                m_ShaderType = ShaderType::Standard;
                if (type == nvrhi::ShaderType::Vertex)
                    m_VertexShader = handle;
                else if (type == nvrhi::ShaderType::Pixel)
                    m_PixelShader = handle;
                else if (type == nvrhi::ShaderType::Compute)
                {
                    m_ComputeShader = handle;
                    m_ShaderType = ShaderType::Compute;
                }
            }
            IncrementVersion();
        }
        else
        {
            LX_CORE_ERROR("Could not open shader file '{0}'", m_FilePath);
        }
    }

    std::unordered_map<nvrhi::ShaderType, std::string> Shader::PreProcess(const std::string& source)
    {
        std::unordered_map<nvrhi::ShaderType, std::string> result;

        const char* typeToken = "#type";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);
        while (pos != std::string::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            size_t begin = pos + typeTokenLength + 1;
            std::string type = source.substr(begin, eol - begin);
            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            pos = source.find(typeToken, nextLinePos);

            std::string shaderCode = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);

            if (type == "vertex")
                result[nvrhi::ShaderType::Vertex] = shaderCode;
            else if (type == "pixel" || type == "fragment")
                result[nvrhi::ShaderType::Pixel] = shaderCode;
            else if (type == "compute")
                result[nvrhi::ShaderType::Compute] = shaderCode;
        }

        return result;
    }
}
