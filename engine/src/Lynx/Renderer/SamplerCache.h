#pragma once
#include "Lynx/Asset/TextureSpecification.h"

#include <vulkan/vulkan.hpp>

#include <memory>
#include <unordered_map>

namespace Lynx
{
    // Makes one sampler for every combination of sampler settings that is asked for, and keeps it.
    // Samplers live as long as the cache: there are only a handful of them.
    class LX_API SamplerCache
    {
    public:
        static void Init(vk::Device device, float maxAnisotropy = 16.0f);
        static void Shutdown();

        static SamplerCache* Get();

        vk::Sampler GetSampler(const SamplerSettings& settings);

        SamplerCache(vk::Device device, float maxAnisotropy);
        ~SamplerCache();

        SamplerCache(const SamplerCache&) = delete;
        SamplerCache& operator=(const SamplerCache&) = delete;

    private:
        static std::unique_ptr<SamplerCache> s_Instance;

        vk::Device m_Device;
        float m_MaxAnisotropy = 16.0f;
        std::unordered_map<SamplerSettings, vk::Sampler> m_Samplers;
    };
}
