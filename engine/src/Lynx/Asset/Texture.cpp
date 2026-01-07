#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Lynx/Engine.h"

namespace Lynx
{
    Texture::Texture(const std::string& filepath, const TextureSpecification& spec)
        : Asset(filepath), m_Specification(spec)
    {
    }

    Texture::Texture(std::vector<uint8_t> data, uint32_t width, uint32_t height, const std::string& debugName)
    {
        m_Specification.DebugName = debugName;
        m_Specification.Width = width;
        m_Specification.Height = height;
        m_PixelData = data.data();
        if (Texture::CreateRenderResources())
            m_State = AssetState::Ready;
        else
            m_State = AssetState::Error;
    }

    Texture::~Texture()
    {
        if (m_PixelData)
        {
            stbi_image_free(m_PixelData);
            m_PixelData = nullptr;
        }
    }

    bool Texture::LoadSourceData()
    {
        if (!std::filesystem::exists(m_FilePath))
        {
            LX_CORE_ERROR("File does not exist: {0}", m_FilePath);
            return false;
        }

        int width, height, channels;
        m_PixelData = stbi_load(m_FilePath.c_str(), &width, &height, &channels, 0);

        if (!m_PixelData)
        {
            LX_CORE_ERROR("Failed to load texture: {0}", m_FilePath);
            return false;
        }

        m_Specification.Width = width;
        m_Specification.Height = height;
        m_Specification.DebugName = m_FilePath;

        if (channels == 4)
        {
            m_Specification.Format = TextureFormat::RGBA8;
        }
        else if (channels == 1)
        {
            m_Specification.Format = TextureFormat::R8;
        }
        else
        {
            LX_CORE_WARN("Unsupported number of channels '{0}'. Forcing 4 channels...", channels);
            stbi_image_free(m_PixelData);
            m_PixelData = stbi_load(m_FilePath.c_str(), &width, &height, &channels, 4);
            m_Specification.Format = TextureFormat::RGBA8;
        }

        return true;
    }

    bool Texture::CreateRenderResources()
    {
        if (!m_PixelData)
            return false;
        
        m_TextureHandle = Engine::Get().GetRenderer().CreateTexture(m_Specification, m_PixelData);

        if (!m_FilePath.empty())
            stbi_image_free(m_PixelData);
        
        m_PixelData = nullptr;
        
        if (m_TextureHandle)
        {
            LX_CORE_TRACE("Texture loaded successfully: {0} ({1}x{2})", m_FilePath, GetWidth(), GetHeight());
            IncrementVersion();
            return true;
        }

        return false;
    }

    bool Texture::Reload()
    {
        // TODO: This could be done async too. Add later and keep old texture handle alive until new one is loaded.
        if (LoadSourceData())
        {
            m_TextureHandle = nullptr;
            return CreateRenderResources();
        }
        return false;
    }
}
