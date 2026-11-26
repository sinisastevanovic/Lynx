#include "Texture.h"
#include "TextureGuess.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Lynx/Engine.h"
#include "TextureCache.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"

namespace Lynx
{
    namespace
    {
        // Every size of the texture in a file, cooked with the settings, from the cache where the
        // file was cooked with them before. The settings take the texture's size and format, and a
        // guess at what it holds where they did not say. Uses nothing of a texture, so any thread
        // may run it for one that is in use.
        bool ReadTextureFile(const std::string& filePath, TextureSpecification& settings, CookedTexture& cooked, bool& hasGuessedSettings)
        {
            if (!std::filesystem::exists(filePath))
            {
                LX_CORE_ERROR("File does not exist: {0}", filePath);
                return false;
            }

            settings.DebugName = filePath;
            hasGuessedSettings = false;

            // A texture that was cooked before, from this file as it is and with these settings, is read
            // as it was cooked. Settings that do not say what the texture holds cannot have been cooked with.
            const TextureCache& cache = Engine::Get().GetTextureCache();
            if (settings.Content != TextureContent::Undecided)
            {
                if (std::optional<CookedTexture> cached = cache.Load(filePath, TextureCookingKey(settings)))
                {
                    cooked = std::move(*cached);
                    settings.Width = cooked.Width();
                    settings.Height = cooked.Height();
                    settings.Format = cooked.Format;
                    return true;
                }
            }

            // A file with one channel stays as it is. Every other one is read as four: GPUs have no texture
            // format with three channels, and most pictures, every JPEG among them, are stored with three.
            // The header says how many the file has, so the pixels are decoded once, in the form they are used in.
            int width, height, channelsInFile;
            if (!stbi_info(filePath.c_str(), &width, &height, &channelsInFile))
            {
                LX_CORE_ERROR("Failed to load texture: {0}", filePath);
                return false;
            }

            const int channels = channelsInFile == 1 ? 1 : 4;
            unsigned char* decoded = stbi_load(filePath.c_str(), &width, &height, &channelsInFile, channels);
            if (!decoded)
            {
                LX_CORE_ERROR("Failed to load texture: {0}", filePath);
                return false;
            }
            const std::span<const uint8_t> pixels(decoded, static_cast<size_t>(width) * height * channels);

            // Settings that do not say what the texture holds get a guess from the picture itself.
            hasGuessedSettings = settings.Content == TextureContent::Undecided;
            if (hasGuessedSettings)
            {
                const TextureGuess guess = GuessTextureSettings(pixels, width, height, channels, filePath);
                settings.Content = guess.Content;
                settings.KeepsCoverage = guess.KeepsCoverage;
            }

            cooked = CookTexture(pixels, width, height, channels, settings);
            stbi_image_free(decoded);
            if (cooked.Sizes.empty())
            {
                LX_CORE_ERROR("Failed to cook texture: {0}", filePath);
                return false;
            }
            cache.Store(filePath, TextureCookingKey(settings), cooked);

            settings.Width = cooked.Width();
            settings.Height = cooked.Height();
            settings.Format = cooked.Format;
            return true;
        }

        // A texture's file read anew, with the settings the texture had when the reload began.
        class TextureReload : public AssetReload
        {
        public:
            TextureReload(std::string filePath, const TextureSpecification& settings)
                : Settings(settings), m_FilePath(std::move(filePath))
            {
            }

            bool Read() override { return ReadTextureFile(m_FilePath, Settings, Cooked, HasGuessedSettings); }

            TextureSpecification Settings;
            CookedTexture Cooked;
            bool HasGuessedSettings = false;

        private:
            std::string m_FilePath;
        };
    }

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
        return ReadTextureFile(m_FilePath, m_Specification, m_Cooked, m_HasGuessedSettings);
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

        if (m_GpuTexture)
        {
            // A texture made in memory has its smaller sizes made on the GPU, which take a third more.
            const uint64_t largest = static_cast<uint64_t>(GetWidth()) * GetHeight() * (m_Specification.Format == TextureFormat::R8 ? 1 : 4);
            m_SizeCount = m_GpuTexture->Desc().MipLevels;
            m_ByteCount = !m_Cooked.Sizes.empty() ? m_Cooked.Pixels.size() : m_SizeCount > 1 ? largest * 4 / 3 : largest;
        }

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
        return LoadSourceData() && CreateRenderResources();
    }

    std::unique_ptr<AssetReload> Texture::BeginReload()
    {
        // A texture made from pixels in memory has no file to read anew.
        if (m_FilePath.empty())
            return nullptr;

        return std::make_unique<TextureReload>(m_FilePath, m_Specification);
    }

    bool Texture::FinishReload(AssetReload& reload)
    {
        TextureReload& read = static_cast<TextureReload&>(reload);
        m_Specification = read.Settings;
        m_Cooked = std::move(read.Cooked);
        m_HasGuessedSettings = read.HasGuessedSettings;
        return CreateRenderResources();
    }
}
