#pragma once
#include "Lynx/Asset/TextureSpecification.h"
#include "nvrhi/nvrhi.h"

namespace Lynx
{
    class LX_API SamplerCache
    {
    public:
        static void Init(nvrhi::DeviceHandle device, float maxAnisotropy = 16.0f);
        static void Shutdown();

        static SamplerCache* Get();
        
        nvrhi::SamplerHandle GetSampler(const SamplerSettings& settings);

        SamplerCache(nvrhi::DeviceHandle device, float maxAnisotropy);
        ~SamplerCache() = default;

    private:
        static std::unique_ptr<SamplerCache> s_Instance;
        
        nvrhi::DeviceHandle m_Device;
        float m_MaxAnisotropy = 16.0f;
        std::unordered_map<SamplerSettings, nvrhi::SamplerHandle> m_SamplerCache;
    };
}
