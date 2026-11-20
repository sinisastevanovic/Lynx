#pragma once
#include "Asset.h"
#include <nvrhi/nvrhi.h>

namespace Lynx
{
    struct ShaderProgram;

    enum class ShaderType
    {
        Standard,
        Compute
    };
    
    class LX_API Shader : public Asset
    {
    public:
        Shader(const std::string& filePath);
        ~Shader() = default;

        static AssetType GetStaticType() { return AssetType::Shader; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        virtual bool Reload() override;

        nvrhi::ShaderHandle GetVertexShader() const { return m_VertexShader; }
        nvrhi::ShaderHandle GetPixelShader() const { return m_PixelShader; }
        nvrhi::ShaderHandle GetComputeShader() const { return m_ComputeShader; }

        // The compiled program of a .slang file, with what it expects to be bound. Null for a .glsl file,
        // and null if the file has never compiled. After a failed reload it is still the last one that compiled.
        std::shared_ptr<const ShaderProgram> GetProgram() const { return m_Program; }

    private:
        void LoadAndCompile();
        std::unordered_map<nvrhi::ShaderType, std::string> PreProcess(const std::string& source);

    private:
        ShaderType m_ShaderType = ShaderType::Standard;
        
        nvrhi::ShaderHandle m_VertexShader;
        nvrhi::ShaderHandle m_PixelShader;
        nvrhi::ShaderHandle m_ComputeShader;

        std::shared_ptr<const ShaderProgram> m_Program;
    };

}
