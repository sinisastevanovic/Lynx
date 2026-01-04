#include "SamplerCache.h"

namespace Lynx
{
    std::unique_ptr<SamplerCache> SamplerCache::s_Instance = nullptr;
    
    static nvrhi::SamplerAddressMode WrapModeToNvrhi(TextureWrap wrap)
    {
        switch (wrap)
        {
            case TextureWrap::Repeat: return nvrhi::SamplerAddressMode::Repeat;
            case TextureWrap::Clamp: return nvrhi::SamplerAddressMode::Clamp;
            case TextureWrap::Mirror: return nvrhi::SamplerAddressMode::Mirror;
        }
        return nvrhi::SamplerAddressMode::Repeat;
    }

    SamplerCache::SamplerCache(nvrhi::DeviceHandle device, float maxAnisotropy)
        : m_Device(device), m_MaxAnisotropy(maxAnisotropy) {}

    
    void SamplerCache::Init(nvrhi::DeviceHandle device, float maxAnisotropy)
    {
        s_Instance = std::unique_ptr<SamplerCache>(new SamplerCache(device, maxAnisotropy));
    }

    void SamplerCache::Shutdown()
    {
        s_Instance.reset();
    }

    SamplerCache* SamplerCache::Get()
    {
        return s_Instance.get();
    }

    nvrhi::SamplerHandle SamplerCache::GetSampler(const SamplerSettings& settings)
    {
        auto it = m_SamplerCache.find(settings);
        if (it != m_SamplerCache.end())
            return it->second;

        // Create a new Sampler
        auto desc = nvrhi::SamplerDesc();

        nvrhi::SamplerAddressMode addressMode = WrapModeToNvrhi(settings.WrapMode);
        desc.setAddressU(addressMode).setAddressV(addressMode).setAddressW(addressMode);

        if (settings.FilterMode == TextureFilter::Nearest)
            desc.setMinFilter(false).setMagFilter(false).setMipFilter(false);
        else if (settings.FilterMode == TextureFilter::Bilinear)
            desc.setMinFilter(true).setMagFilter(true).setMipFilter(false);
        else if (settings.FilterMode == TextureFilter::Trilinear)
        {
            desc.setMinFilter(true).setMagFilter(true).setMipFilter(true);
            if (settings.UseAnisotropy)
                desc.setMaxAnisotropy(m_MaxAnisotropy);
            else
                desc.setMaxAnisotropy(1.0f);
        }

        nvrhi::SamplerHandle handle = m_Device->createSampler(desc);
        m_SamplerCache[settings] = handle;
        return handle;
    }
}
