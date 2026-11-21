#pragma once
#include "Asset.h"

namespace Lynx
{
    struct ShaderProgram;

    // A shader file written in Slang. Compiling it gives a program for the renderer's passes to make pipelines from.
    class LX_API Shader : public Asset
    {
    public:
        Shader(const std::string& filePath);
        ~Shader() = default;

        static AssetType GetStaticType() { return AssetType::Shader; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        virtual bool Reload() override;

        // The compiled program, with what it expects to be bound. Null if the file has never compiled.
        // After a failed reload it is still the last one that compiled.
        std::shared_ptr<const ShaderProgram> GetProgram() const { return m_Program; }

    private:
        void LoadAndCompile();

    private:
        std::shared_ptr<const ShaderProgram> m_Program;
    };
}
