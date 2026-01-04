#include "MaterialSerializer.h"
#include <nlohmann/json.hpp>

namespace Lynx
{
    bool MaterialSerializer::Serialize(const std::filesystem::path& filePath, const std::shared_ptr<Material>& material)
    {
        nlohmann::json json;

        // Serialize Properties
        json["AlbedoColor"] = { material->AlbedoColor.r, material->AlbedoColor.g, material->AlbedoColor.b, material->AlbedoColor.a };
        json["Metallic"] = material->Metallic;
        json["Roughness"] = material->Roughness;
        json["EmissiveColor"] = { material->EmissiveColor.r, material->EmissiveColor.g, material->EmissiveColor.b };
        json["EmissiveStrength"] = material->EmissiveStrength;
        json["AlphaCutoff"] = material->AlphaCutoff;

        // Serialize Enums (as int for simplicity, or string for readability)
        json["Mode"] = (int)material->Mode;

        // Serialize Textures (Handles)
        json["UseNormalMap"] = material->UseNormalMap;

        auto& textures = json["Textures"];
        textures["Albedo"] = (uint64_t)material->AlbedoTexture;
        textures["Normal"] = (uint64_t)material->NormalMap;
        textures["MetallicRoughness"] = (uint64_t)material->MetallicRoughnessTexture;
        textures["Emissive"] = (uint64_t)material->EmissiveTexture;
        textures["Occlusion"] = (uint64_t)material->OcclusionTexture;

        std::ofstream fout(filePath);
        if (fout.is_open())
        {
            fout << json.dump(4);
            fout.close();
        }
        else
        {
            LX_CORE_ERROR("Could not save material to: {0}", filePath.string());
            return false;
        }
        return true;
    }

    bool MaterialSerializer::Deserialize(const std::filesystem::path& filePath, Material& outMaterial)
    {
        std::ifstream stream(filePath);
        if (!stream.is_open())
        {
            LX_CORE_ERROR("Could not open material file: {0}", filePath.string());
            return false;
        }

        nlohmann::json json;
        try
        {
            stream >> json;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            LX_CORE_ERROR("Failed to parse material JSON: {0}", e.what());
            return false;
        }

        // Helpers
        auto getVec3 = [](const nlohmann::json& j) { return glm::vec3(j[0], j[1], j[2]); };
        auto getVec4 = [](const nlohmann::json& j) { return glm::vec4(j[0], j[1], j[2], j[3]); };

        // Deserialize Properties
        if (json.contains("AlbedoColor")) outMaterial.AlbedoColor = getVec4(json["AlbedoColor"]);
        if (json.contains("Metallic")) outMaterial.Metallic = json["Metallic"];
        if (json.contains("Roughness")) outMaterial.Roughness = json["Roughness"];
        if (json.contains("EmissiveColor")) outMaterial.EmissiveColor = getVec3(json["EmissiveColor"]);
        if (json.contains("EmissiveStrength")) outMaterial.EmissiveStrength = json["EmissiveStrength"];
        if (json.contains("AlphaCutoff")) outMaterial.AlphaCutoff = json["AlphaCutoff"];

        if (json.contains("Mode")) outMaterial.Mode = (AlphaMode)json["Mode"];
        if (json.contains("UseNormalMap")) outMaterial.UseNormalMap = json["UseNormalMap"];

        if (json.contains("Textures"))
        {
            auto& textures = json["Textures"];
            if (textures.contains("Albedo")) outMaterial.AlbedoTexture = AssetHandle(textures["Albedo"].get<uint64_t>());
            if (textures.contains("Normal")) outMaterial.NormalMap = AssetHandle(textures["Normal"].get<uint64_t>());
            if (textures.contains("MetallicRoughness")) outMaterial.MetallicRoughnessTexture = AssetHandle(textures["MetallicRoughness"].get<uint64_t>());
            if (textures.contains("Emissive")) outMaterial.EmissiveTexture = AssetHandle(textures["Emissive"].get<uint64_t>());
            if (textures.contains("Occlusion")) outMaterial.OcclusionTexture = AssetHandle(textures["Occlusion"].get<uint64_t>());
        }

        return true;
    }
}
