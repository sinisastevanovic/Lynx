#pragma once

#include "Asset.h"

namespace Lynx
{
    class Script : public Asset
    {
    public:
        Script(const std::string& filePath) : m_FilePath(filePath) {}

        static AssetType GetStaticType() { return AssetType::Script; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        virtual bool Reload() override;

        std::string GetFilePath() const { return m_FilePath; }
    private:
        std::string m_FilePath;
    };
}