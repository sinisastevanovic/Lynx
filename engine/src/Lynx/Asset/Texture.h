#pragma once
#include "Lynx/Core.h"
#include "Asset.h"

#include "TextureCooking.h"
#include "TextureSpecification.h"
#include "Lynx/Renderer/TextureSlot.h"

#include <memory>

namespace Lynx
{
    class GpuTexture;

    class LX_API Texture : public Asset
    {
    public:
        Texture(const std::string& filepath, const TextureSpecification& spec);
        Texture(std::vector<uint8_t> data, uint32_t width, uint32_t height, const std::string& debugName = "");

        ~Texture() override;

        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetType() const override { return GetStaticType(); }

        // True once the picture exists on the GPU. Until then the texture cannot be drawn with.
        bool IsOnGpu() const { return m_GpuTexture != nullptr; }

        // Where the renderer's texture and sampler tables have this texture and its sampler.
        // Valid once the texture is on the GPU.
        uint32_t GetTableIndex() const { return m_TableSlot.Index(); }

        // The same texture read as colours painted for the screen (sRGB): sampling it gives linear light,
        // which is what lighting computes with. For a material's albedo and emissive textures.
        // GetTableIndex gives the stored numbers as they are: for normal maps, roughness, and for pictures
        // that go to the screen unlit, like the UI's.
        uint32_t GetColorTableIndex() const { return m_ColorTableSlot ? m_ColorTableSlot.Index() : m_TableSlot.Index(); }

        // Where the texture table has what a cut-out is tested from: the texture's finest sizes alone
        // if its alpha cuts shapes out (TextureSpecification::KeepsCoverage), the whole texture otherwise.
        uint32_t GetCutOutTableIndex() const { return m_CutOutTableSlot ? m_CutOutTableSlot.Index() : GetColorTableIndex(); }
        uint32_t GetSamplerIndex() const { return m_SamplerIndex; }

        uint32_t GetWidth() const { return m_Specification.Width; }
        uint32_t GetHeight() const { return m_Specification.Height; }

        // In how many sizes the texture is on the GPU, each half the one before, and how many bytes
        // all of them take there together. Valid once the texture is on the GPU.
        uint32_t GetSizeCount() const { return m_SizeCount; }
        uint64_t GetByteCount() const { return m_ByteCount; }

        void SetSpecification(const TextureSpecification& spec) { m_Specification = spec; }
        const TextureSpecification& GetSpecification() const { return m_Specification; }
        const SamplerSettings& GetSamplerSettings() const { return m_Specification.SamplerSettings; }

        // True if loading had to guess what the texture holds, because its settings did not say.
        // Whoever keeps the settings saves the guess then, so that it is made once and can be changed.
        bool HasGuessedSettings() const { return m_HasGuessedSettings; }

        virtual bool LoadSourceData() override;
        virtual bool CreateRenderResources() override;
        virtual bool Reload() override;

        // A texture from a file is read, cooked and compressed anew on a worker thread, which takes
        // seconds for a large one, and stays as it was until then.
        virtual std::unique_ptr<AssetReload> BeginReload() override;
        virtual bool FinishReload(AssetReload& reload) override;

    private:
        TextureSpecification m_Specification;
        std::unique_ptr<GpuTexture> m_GpuTexture;
        TextureSlot m_TableSlot;

        // Empty for a format without an sRGB form, such as a font's single channel.
        TextureSlot m_ColorTableSlot;

        // Empty for a texture whose alpha cuts nothing out.
        TextureSlot m_CutOutTableSlot;
        uint32_t m_SamplerIndex = 0;
        bool m_HasGuessedSettings = false;
        uint32_t m_SizeCount = 1;
        uint64_t m_ByteCount = 0;

        // A texture made from pixels in memory: they are handed to the GPU as they are.
        unsigned char* m_PixelData = nullptr;

        // A texture from a file, between loading and reaching the GPU: every size of it, see TextureCooking.
        CookedTexture m_Cooked;
    };
}

