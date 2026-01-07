#include "SpriteSerializer.h"
#include <nlohmann/json.hpp>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Texture.h"

namespace Lynx
{
    bool SpriteSerializer::Serialize(const std::filesystem::path& filePath, const std::shared_ptr<Sprite>& sprite)
    {
        nlohmann::json json;

        json["UVMin"] = sprite->GetUVMin();
        json["UVMax"] = sprite->GetUVMax();
        UIThickness border = sprite->GetBorder();
        json["Border"] = sprite->GetBorder();
        json["Texture"] = sprite->GetTexture() ? sprite->GetTexture()->GetHandle() : AssetHandle::Null();

        std::ofstream fout(filePath);
        if (fout.is_open())
        {
            fout << json.dump(4);
            fout.close();
        }
        else
        {
            LX_CORE_ERROR("Could not save sprite to: {0}", filePath.string());
            return false;
        }
        return true;
    }

    bool SpriteSerializer::Deserialize(const std::filesystem::path& filePath, Sprite& outSprite)
    {
        std::ifstream stream(filePath);
        if (!stream.is_open())
        {
            LX_CORE_ERROR("Could not open sprite file: {0}", filePath.string());
            return false;
        }

        nlohmann::json json;
        try
        {
            stream >> json;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            LX_CORE_ERROR("Failed to parse sprite JSON: {0}", e.what());
            return false;
        }

        if (json.contains("UVMin") && json.contains("UVMax"))
            outSprite.SetUVs(json["UVMin"].get<glm::vec2>(), json["UVMax"].get<glm::vec2>());

        if (json.contains("Border"))
            outSprite.SetBorder(json["Border"].get<UIThickness>());

        if (json.contains("Texture"))
        {
            AssetHandle textureHandle = json["Texture"].get<AssetHandle>();
            if (textureHandle.IsValid())
            {
                outSprite.SetTexture(Engine::Get().GetAssetManager().GetAsset<Texture>(textureHandle));
            }
        }
        
        return true;
    }
}
