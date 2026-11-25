#pragma once

#include <cstdint>

namespace Lynx
{
    class SharedBindings;
    enum class TextureTable : uint8_t;

    // The place of one texture in one of the texture tables of all shaders: that of flat pictures, that of
    // cubes, or that of the pictures compute shaders write into. Owns the place: when the
    // object goes away, the index is given back once the frames that may still use it are done.
    // The texture itself outlives those frames on its own, see GpuTexture.
    class TextureSlot
    {
    public:
        TextureSlot() = default;
        // Takes over an index the caller got from SharedBindings::AddTexture, or from AddCubeTexture
        // or AddWritableTexture if the table named is that of cubes or of writable pictures.
        TextureSlot(SharedBindings& shared, uint32_t index);
        TextureSlot(SharedBindings& shared, uint32_t index, TextureTable table);
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
        uint32_t m_Index = 0;
        TextureTable m_Table{};
    };
}
