#pragma once

#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace Lynx
{
    class SharedBindings;

    // The place of one texture in the texture table that all shaders read from. Owns the place: when the
    // object goes away, the index is given back once the frames that may still use it are done.
    //
    // For an NVRHI texture it also keeps the texture alive for that long. Without it, a texture asset
    // that is unloaded would destroy a picture that a frame on the GPU is still reading. A texture of
    // the Vulkan layer outlives those frames by itself, so its slot only holds the index.
    class TextureSlot
    {
    public:
        TextureSlot() = default;
        TextureSlot(SharedBindings& shared, nvrhi::TextureHandle texture);

        // Takes over an index the caller got from SharedBindings::AddTexture.
        TextureSlot(SharedBindings& shared, uint32_t index);
        ~TextureSlot();

        TextureSlot(TextureSlot&& other) noexcept;
        TextureSlot& operator=(TextureSlot&& other) noexcept;
        TextureSlot(const TextureSlot&) = delete;
        TextureSlot& operator=(const TextureSlot&) = delete;

        explicit operator bool() const { return m_Shared != nullptr; }

        // Only meaningful while the slot holds a texture.
        uint32_t Index() const { return m_Index; }

    private:
        void Release();

    private:
        SharedBindings* m_Shared = nullptr;
        nvrhi::TextureHandle m_Texture;
        uint32_t m_Index = 0;
    };
}
