#pragma once
#include <nlohmann/json.hpp>

#include "AssetTypes.h"

namespace Lynx
{
    struct LX_API AssetSpecification
    {
    public:
        virtual ~AssetSpecification() = default;

        virtual void Serialize(nlohmann::json& json) const = 0;
        virtual void Deserialize(const nlohmann::json& json) = 0;

        virtual uint32_t GetCurrentVersion() const { return 1; }

        static std::shared_ptr<AssetSpecification> CreateFromType(AssetType type);
    };
}
