#pragma once

#include "Asset.h"

namespace Lynx
{
    class Script : public Asset
    {
    public:
        Script(const std::string& filePath) : Asset(filePath) {}

        static AssetType GetStaticType() { return AssetType::Script; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        virtual bool Reload() override;
    };
}