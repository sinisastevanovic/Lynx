#pragma once
#include "Asset.h"

namespace Lynx
{
    class LX_API Prefab : public Asset
    {
    public:
        Prefab(const std::string& filePath);
        virtual ~Prefab() override = default;
        
        static AssetType GetStaticType() { return AssetType::Prefab; }
        AssetType GetType() const override { return GetStaticType(); }
        
        const nlohmann::json& GetData() const { return m_Data; }
        
        bool LoadSourceData() override;
        bool Reload() override;

    private:
        nlohmann::json m_Data;
    };
}

