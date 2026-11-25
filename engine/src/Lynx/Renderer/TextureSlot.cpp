#include "TextureSlot.h"

#include "Vulkan/SharedBindings.h"

#include <utility>

namespace Lynx
{
    TextureSlot::TextureSlot(SharedBindings& shared, uint32_t index)
        : m_Shared(&shared), m_Index(index)
    {
    }

    TextureSlot::TextureSlot(SharedBindings& shared, uint32_t index, TextureTable table)
        : m_Shared(&shared), m_Index(index), m_IsCube(table == TextureTable::Cube)
    {
    }

    TextureSlot::~TextureSlot()
    {
        Release();
    }

    TextureSlot::TextureSlot(TextureSlot&& other) noexcept
        : m_Shared(std::exchange(other.m_Shared, nullptr)),
          m_Index(std::exchange(other.m_Index, 0)),
          m_IsCube(std::exchange(other.m_IsCube, false))
    {
    }

    TextureSlot& TextureSlot::operator=(TextureSlot&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Shared = std::exchange(other.m_Shared, nullptr);
            m_Index = std::exchange(other.m_Index, 0);
            m_IsCube = std::exchange(other.m_IsCube, false);
        }
        return *this;
    }

    void TextureSlot::Release()
    {
        if (!m_Shared)
            return;

        m_Shared->ReleaseTexture(m_Index, m_IsCube ? TextureTable::Cube : TextureTable::Flat);
        m_Shared = nullptr;
        m_Index = 0;
        m_IsCube = false;
    }
}
