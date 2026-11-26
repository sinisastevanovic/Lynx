#include "Texture.h"
#include "TextureGuess.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Lynx/Engine.h"
#include "TextureCache.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"

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

    Texture::~Texture() = default;

    bool Texture::LoadSourceData()
    {
        if (!std::filesystem::exists(m_FilePath))
        {
            LX_CORE_ERROR("File does not exist: {0}", m_FilePath);
            return false;
        }

        m_Specification.DebugName = m_FilePath;
        m_HasGuessedSettings = false;

        // A texture that was cooked before, from this file as it is and with these settings, is read
        // as it was cooked. Settings that do not say what the texture holds cannot have been cooked with.
        const TextureCache& cache = Engine::Get().GetTextureCache();
        if (m_Specification.Content != TextureContent::Undecided)
        {
            if (std::optional<CookedTexture> cooked = cache.Load(m_FilePath, TextureCookingKey(m_Specification)))
            {
                m_Cooked = std::move(*cooked);
                m_Specification.Width = m_Cooked.Width();
                m_Specification.Height = m_Cooked.Height();
                m_Specification.Format = m_Cooked.Format;
                return true;
            }
        }

        // A file with one channel stays as it is. Every other one is read as four: GPUs have no texture
        // format with three channels, and most pictures, every JPEG among them, are stored with three.
        // The header says how many the file has, so the pixels are decoded once, in the form they are used in.
        int width, height, channelsInFile;
        if (!stbi_info(m_FilePath.c_str(), &width, &height, &channelsInFile))
        {
            LX_CORE_ERROR("Failed to load texture: {0}", m_FilePath);
            return false;
        }

        const int channels = channelsInFile == 1 ? 1 : 4;
        unsigned char* decoded = stbi_load(m_FilePath.c_str(), &width, &height, &channelsInFile, channels);
        if (!decoded)
        {
            LX_CORE_ERROR("Failed to load texture: {0}", m_FilePath);
            return false;
        }
        const std::span<const uint8_t> pixels(decoded, static_cast<size_t>(width) * height * channels);

        // Settings that do not say what the texture holds get a guess from the picture itself.
        m_HasGuessedSettings = m_Specification.Content == TextureContent::Undecided;
        if (m_HasGuessedSettings)
        {
            const TextureGuess guess = GuessTextureSettings(pixels, width, height, channels, m_FilePath);
            m_Specification.Content = guess.Content;
            m_Specification.KeepsCoverage = guess.KeepsCoverage;
        }

        m_Cooked = CookTexture(pixels, width, height, channels, m_Specification);
        stbi_image_free(decoded);
        if (m_Cooked.Sizes.empty())
        {
            LX_CORE_ERROR("Failed to cook texture: {0}", m_FilePath);
            return false;
        }
        cache.Store(m_FilePath, TextureCookingKey(m_Specification), m_Cooked);

        m_Specification.Width = m_Cooked.Width();
        m_Specification.Height = m_Cooked.Height();
        m_Specification.Format = m_Cooked.Format;
        return true;
    }

    bool Texture::CreateRenderResources()
    {
        if (!m_PixelData && m_Cooked.Sizes.empty())
            return false;

        Renderer& renderer = Engine::Get().GetRenderer();

        // The places in the table go before the picture they point at.
        m_TableSlot = TextureSlot();
        m_ColorTableSlot = TextureSlot();
        m_CutOutTableSlot = TextureSlot();
        m_GpuTexture = m_PixelData ? renderer.CreateTexture(m_Specification, m_PixelData) : renderer.CreateTexture(m_Specification, m_Cooked);

        // The pixels are the GPU's from here on. A texture made in memory never owned its own.
        m_PixelData = nullptr;
        m_Cooked = CookedTexture();

        if (m_GpuTexture)
        {
            m_TableSlot = renderer.AddToTextureTable(*m_GpuTexture);
            m_ColorTableSlot = renderer.AddColorToTextureTable(*m_GpuTexture);
            m_CutOutTableSlot = renderer.AddFinestSizesToTextureTable(*m_GpuTexture);
            m_SamplerIndex = renderer.SamplerIndex(m_Specification.SamplerSettings);
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
            return CreateRenderResources();
        }
        return false;
    }
}
