#pragma once
#include "AssetSpecification.h"

namespace Lynx
{
    struct StaticMeshSpecification : public AssetSpecification
    {
        // Import Options
        bool CalculateNormals = false;
        bool CalculateTangents = false;
        bool FlipUVs = false;
        float GlobalScale = 1.0f;

        // Runtime Settings
        bool KeepCPUData = false; // Do we keep vertices in RAM after upload? (For physics/picking)

        std::string DebugName = "Mesh";

        virtual void Serialize(nlohmann::json& json) const override
        {
            /*json["TextureFormat"] = Format;
            json["WrapMode"] = SamplerSettings.WrapMode;
            json["FilterMode"] = SamplerSettings.FilterMode;
            json["IsSRGB"] = IsSRGB;
            json["GenerateMips"] = GenerateMips;*/
        }

        virtual void Deserialize(const nlohmann::json& json) override
        {
            /*Format = (TextureFormat)json["TextureFormat"];
            SamplerSettings.WrapMode = (TextureWrap)json["WrapMode"];
            SamplerSettings.FilterMode = (TextureFilter)json["FilterMode"];
            IsSRGB = json["IsSRGB"];
            GenerateMips = json["GenerateMips"];*/
        }
    };
}
