#include "Sprite.h"

#include "Texture.h"
#include "Serialization/SpriteSerializer.h"

namespace Lynx
{
    Sprite::Sprite(const std::string& filePath)
        : Asset(filePath)
    {
        Sprite::Reload();
    }

    bool Sprite::LoadSourceData()
    {
        if (m_FilePath.empty())
            return true;

        return SpriteSerializer::Deserialize(m_FilePath, *this);
    }

    bool Sprite::Reload()
    {
        return LoadSourceData();
    }

    bool Sprite::DependsOn(AssetHandle handle) const
    {
        return m_Texture ? m_Texture->GetHandle() == handle : false;
    }
}
