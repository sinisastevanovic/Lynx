#include "SamplerCache.h"

#include <limits>

namespace Lynx
{
    std::unique_ptr<SamplerCache> SamplerCache::s_Instance = nullptr;

    namespace
    {
        vk::SamplerAddressMode ToVulkan(TextureWrap wrap)
        {
            switch (wrap)
            {
                case TextureWrap::Repeat: return vk::SamplerAddressMode::eRepeat;
                case TextureWrap::Clamp:  return vk::SamplerAddressMode::eClampToEdge;
                case TextureWrap::Mirror: return vk::SamplerAddressMode::eMirroredRepeat;
            }
            return vk::SamplerAddressMode::eRepeat;
        }
    }

    SamplerCache::SamplerCache(vk::Device device, float maxAnisotropy)
        : m_Device(device), m_MaxAnisotropy(maxAnisotropy)
    {
    }

    SamplerCache::~SamplerCache()
    {
        // The renderer waits for the GPU before it shuts the cache down, so no sampler is still in use.
        for (const auto& [settings, sampler] : m_Samplers)
            m_Device.destroySampler(sampler);
    }

    void SamplerCache::Init(vk::Device device, float maxAnisotropy)
    {
        s_Instance = std::make_unique<SamplerCache>(device, maxAnisotropy);
    }

    void SamplerCache::Shutdown()
    {
        s_Instance.reset();
    }

    SamplerCache* SamplerCache::Get()
    {
        return s_Instance.get();
    }

    vk::Sampler SamplerCache::GetSampler(const SamplerSettings& settings)
    {
        if (const auto found = m_Samplers.find(settings); found != m_Samplers.end())
            return found->second;

        const bool isLinear = settings.FilterMode != TextureFilter::Nearest;
        const bool isTrilinear = settings.FilterMode == TextureFilter::Trilinear;

        vk::SamplerCreateInfo info;
        info.magFilter = isLinear ? vk::Filter::eLinear : vk::Filter::eNearest;
        info.minFilter = info.magFilter;

        // Only trilinear filtering blends between mip levels. The others take the nearest one.
        info.mipmapMode = isTrilinear ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest;
        info.addressModeU = ToVulkan(settings.WrapMode);
        info.addressModeV = info.addressModeU;
        info.addressModeW = info.addressModeU;
        info.maxLod = std::numeric_limits<float>::max();

        // Sharpens textures seen at a flat angle. Only meaningful together with mip levels.
        if (isTrilinear && settings.UseAnisotropy && m_MaxAnisotropy > 1.0f)
        {
            info.anisotropyEnable = VK_TRUE;
            info.maxAnisotropy = m_MaxAnisotropy;
        }
        else
        {
            info.maxAnisotropy = 1.0f;
        }

        const vk::Sampler sampler = m_Device.createSampler(info);
        m_Samplers.emplace(settings, sampler);
        return sampler;
    }
}
