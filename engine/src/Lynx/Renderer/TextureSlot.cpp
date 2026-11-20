#include "TextureSlot.h"

#include "Vulkan/SharedBindings.h"

#include <utility>

namespace Lynx
{
    TextureSlot::TextureSlot(SharedBindings& shared, nvrhi::TextureHandle texture)
        : m_Shared(&shared), m_Texture(std::move(texture))
    {
        const vk::ImageView view(static_cast<VkImageView>(m_Texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView).pointer));
        m_Index = shared.AddTexture(view);
    }

    TextureSlot::~TextureSlot()
    {
        Release();
    }

    TextureSlot::TextureSlot(TextureSlot&& other) noexcept
        : m_Shared(std::exchange(other.m_Shared, nullptr)),
          m_Texture(std::move(other.m_Texture)),
          m_Index(std::exchange(other.m_Index, 0))
    {
    }

    TextureSlot& TextureSlot::operator=(TextureSlot&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Shared = std::exchange(other.m_Shared, nullptr);
            m_Texture = std::move(other.m_Texture);
            m_Index = std::exchange(other.m_Index, 0);
        }
        return *this;
    }

    void TextureSlot::Release()
    {
        if (!m_Shared)
            return;

        // The texture is let go together with the index, not before.
        m_Shared->ReleaseTexture(m_Index, [texture = std::move(m_Texture)]() {});
        m_Shared = nullptr;
        m_Texture = nullptr;
        m_Index = 0;
    }
}
