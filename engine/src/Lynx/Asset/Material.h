#pragma once
#include "Asset.h"
#include <glm/glm.hpp>

namespace Lynx
{
    enum class AlphaMode { Opaque, Mask, Translucent };
    
    class LX_API Material : public Asset
    {
    public:
        Material(const std::string& filepath);
        Material();
        ~Material() = default;

        static AssetType GetStaticType() { return AssetType::Material; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        glm::vec4 AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        glm::vec3 EmissiveColor = { 0.0f, 0.0f, 0.0f };
        float EmissiveStrength = 0.0f;
        AlphaMode Mode = AlphaMode::Opaque;
        float AlphaCutoff = 0.5f;

        AssetHandle AlbedoTexture = AssetHandle::Null();
        AssetHandle NormalMap = AssetHandle::Null();
        AssetHandle MetallicRoughnessTexture = AssetHandle::Null();
        AssetHandle EmissiveTexture = AssetHandle::Null();
        AssetHandle OcclusionTexture = AssetHandle::Null();

        bool UseNormalMap = false;

        virtual bool Reload() override;

    private:
        std::string m_Filepath;
    };

}
