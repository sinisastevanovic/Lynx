#pragma once
#include "Asset.h"
#include <nvrhi/nvrhi.h>

namespace Lynx
{
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

    private:
        void LoadAndCompile();
        std::unordered_map<nvrhi::ShaderType, std::string> PreProcess(const std::string& source);

    private:
        std::string m_FilePath;
        nvrhi::ShaderHandle m_VertexShader;
        nvrhi::ShaderHandle m_PixelShader;
    };

}
