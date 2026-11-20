#pragma once

#include <nvrhi/nvrhi.h>

#include <cstdint>

namespace Lynx
{
    class SharedBindings;

    // The place of one texture in the texture table that all shaders read from. Owns the place: when the
    // object goes away, the index is given back once the frames that may still use it are done.
    //
    // It also keeps the NVRHI texture behind the index alive for that long. Without it, a texture asset
    // that is unloaded would destroy a picture that a frame on the GPU is still reading.
    class TextureSlot
    {
    public:
        TextureSlot() = default;
        TextureSlot(SharedBindings& shared, nvrhi::TextureHandle texture);
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
