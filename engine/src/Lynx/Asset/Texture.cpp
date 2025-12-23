#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    Texture::Texture(const std::string& filepath)
        : m_FilePath(filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LX_CORE_ERROR("File does not exist: {0}", filepath);
            return;
        }
        
        int width, height, channels;
        stbi_uc* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

        if (!data)
        {
            LX_CORE_ERROR("Failed to load texture: {0}", filepath);
            // TODO: Load "Missing Texture" texture here
            return;
        }

        m_Specification.Width = width;
        m_Specification.Height = height;
        m_Specification.DebugName = filepath;

        m_TextureHandle = Engine::Get().GetRenderer().CreateTexture(m_Specification, data);
        
        stbi_image_free(data);
        if (m_TextureHandle)
            LX_CORE_TRACE("Texture loaded successfully: {0} ({1}x{2})", filepath, width, height);
    }

    Texture::Texture(std::vector<uint8_t> data, uint32_t width, uint32_t height, const std::string& debugName)
    {
        m_Specification.DebugName = debugName;

        CreateTextureFromData(data.data(), width, height);
    }

    bool Texture::Reload()
    {
        if (!std::filesystem::exists(m_FilePath))
        {
            LX_CORE_ERROR("File does not exist: {0}", m_FilePath);
            return false;
        }
        
        int width, height, channels;
        stbi_uc* data = stbi_load(m_FilePath.c_str(), &width, &height, &channels, 4);

        if (!data)
        {
            LX_CORE_ERROR("Failed to load texture: {0}", m_FilePath);
            // TODO: Load "Missing Texture" texture here
            return false;
        }

        m_TextureHandle = nullptr;

        CreateTextureFromData(data, width, height);

        stbi_image_free(data);
        return true;
    }

    void Texture::CreateTextureFromData(unsigned char* data, uint32_t width, uint32_t height)
    {
        m_Specification.Width = width;
        m_Specification.Height = height;

        m_TextureHandle = Engine::Get().GetRenderer().CreateTexture(m_Specification, data);
        if (m_TextureHandle)
            LX_CORE_TRACE("Texture loaded successfully: {0} ({1}x{2})", m_Specification.DebugName, width, height);
    }
}
