#pragma once
#include <nlohmann/json_fwd.hpp>

namespace Lynx
{
    struct LX_API AssetSpecification
    {
    public:
        virtual ~AssetSpecification() = default;

        virtual void Serialize(nlohmann::json& json) const = 0;
        virtual void Deserialize(const nlohmann::json& json) = 0;
    };
}
