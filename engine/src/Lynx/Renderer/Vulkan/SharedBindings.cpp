#include "SharedBindings.h"

#include "ShaderLayout.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/Shader/ShaderProgram.h"

#include <algorithm>
#include <array>

namespace Lynx
{
    SharedBindings::SharedBindings(GraphicsContext& context, vk::Buffer sceneConstants)
        : m_Context(context), m_Device(context.Device()), m_FreeTextures(std::make_shared<std::vector<uint32_t>>())
    {
        const std::array bindings = {
            vk::DescriptorSetLayoutBinding(SceneBinding, vk::DescriptorType::eUniformBuffer, 1, PushConstantStages),
            vk::DescriptorSetLayoutBinding(TextureBinding, vk::DescriptorType::eSampledImage, MaxTextures, PushConstantStages),
            vk::DescriptorSetLayoutBinding(SamplerBinding, vk::DescriptorType::eSampler, MaxSamplers, PushConstantStages),
            vk::DescriptorSetLayoutBinding(ComparisonSamplerBinding, vk::DescriptorType::eSampler, MaxComparisonSamplers, PushConstantStages),
        };

        // The tables are mostly empty, and entries are written while frames that use the set are being
        // recorded or are still on the GPU. Both have to be asked for.
        const vk::DescriptorBindingFlags tableFlags = vk::DescriptorBindingFlagBits::ePartiallyBound
            | vk::DescriptorBindingFlagBits::eUpdateAfterBind
            | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;
        const std::array<vk::DescriptorBindingFlags, 4> bindingFlags = { vk::DescriptorBindingFlags(), tableFlags, tableFlags, tableFlags };

        const vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo(bindingFlags);
        vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, bindings);
        layoutInfo.pNext = &flagsInfo;
        m_SetLayout = m_Device.createDescriptorSetLayout(layoutInfo);

        const vk::PushConstantRange pushConstants(PushConstantStages, 0, MaxPushConstantSize);
        m_PipelineLayout = m_Device.createPipelineLayout(vk::PipelineLayoutCreateInfo({}, m_SetLayout, pushConstants));

        const std::array poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, MaxTextures),
            vk::DescriptorPoolSize(vk::DescriptorType::eSampler, MaxSamplers + MaxComparisonSamplers),
        };
        m_Pool = m_Device.createDescriptorPool(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 1, poolSizes));
        m_Set = m_Device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_Pool, m_SetLayout)).front();

        // The scene constants are the same buffer in every frame, so they are written once.
        const vk::DescriptorBufferInfo sceneInfo(sceneConstants, 0, VK_WHOLE_SIZE);
        m_Device.updateDescriptorSets(vk::WriteDescriptorSet(m_Set, SceneBinding, 0, vk::DescriptorType::eUniformBuffer, {}, sceneInfo), nullptr);
    }

    SharedBindings::~SharedBindings()
    {
        // The renderer waits for the GPU before it destroys this, so nothing here is still in use.
        m_Device.destroyDescriptorPool(m_Pool);
        m_Device.destroyPipelineLayout(m_PipelineLayout);
        m_Device.destroyDescriptorSetLayout(m_SetLayout);
    }

    uint32_t SharedBindings::AddTexture(vk::ImageView view)
    {
        uint32_t index = 0;
        if (!m_FreeTextures->empty())
        {
            index = m_FreeTextures->back();
            m_FreeTextures->pop_back();
        }
        else if (m_TextureCount < MaxTextures)
        {
            index = m_TextureCount++;
        }
        else
        {
            // Index 0 is a valid texture, so the caller draws with the wrong picture instead of crashing.
            LX_CORE_ERROR("The texture table is full ({0} textures).", MaxTextures);
            return 0;
        }

        if (m_TextureViews.size() <= index)
            m_TextureViews.resize(index + 1);
        m_TextureViews[index] = view;

        const vk::DescriptorImageInfo imageInfo({}, view, vk::ImageLayout::eShaderReadOnlyOptimal);
        m_Device.updateDescriptorSets(vk::WriteDescriptorSet(m_Set, TextureBinding, index, vk::DescriptorType::eSampledImage, imageInfo), nullptr);
        return index;
    }

    void SharedBindings::ReleaseTexture(uint32_t index)
    {
        if (m_ReleaseListener)
            m_ReleaseListener(index);

        // A frame on the GPU may still sample through the index. Handing it out again right away
        // would show that frame the next texture.
        m_Context.Deletions().Retire([freeTextures = m_FreeTextures, index]() { freeTextures->push_back(index); });
    }

    uint32_t SharedBindings::SamplerIndex(vk::Sampler sampler)
    {
        return IndexOfSampler(m_Samplers, sampler, SamplerBinding, MaxSamplers);
    }

    uint32_t SharedBindings::ComparisonSamplerIndex(vk::Sampler sampler)
    {
        return IndexOfSampler(m_ComparisonSamplers, sampler, ComparisonSamplerBinding, MaxComparisonSamplers);
    }

    uint32_t SharedBindings::IndexOfSampler(std::vector<vk::Sampler>& table, vk::Sampler sampler, uint32_t binding, uint32_t capacity)
    {
        // There are a handful of samplers, so looking through them is faster than a map.
        const auto found = std::ranges::find(table, sampler);
        if (found != table.end())
            return static_cast<uint32_t>(found - table.begin());

        if (table.size() == capacity)
        {
            LX_CORE_ERROR("A sampler table is full ({0} samplers).", capacity);
            return 0;
        }

        const auto index = static_cast<uint32_t>(table.size());
        table.push_back(sampler);

        const vk::DescriptorImageInfo samplerInfo(sampler);
        m_Device.updateDescriptorSets(vk::WriteDescriptorSet(m_Set, binding, index, vk::DescriptorType::eSampler, samplerInfo), nullptr);
        return index;
    }

    bool SharedBindings::Supports(const ShaderProgram& program, std::string_view shaderName, uint32_t pushConstantSize) const
    {
        if (program.PushConstantSize > MaxPushConstantSize)
        {
            LX_CORE_ERROR("Shader {0}: {1} bytes of push constants, but {2} is the limit.", shaderName, program.PushConstantSize, MaxPushConstantSize);
            return false;
        }

        // A shader pads its constants to a multiple of their largest member, so the two sizes differ
        // if the struct in C++ lacks that padding.
        if (program.PushConstantSize != pushConstantSize)
        {
            LX_CORE_ERROR("Shader {0}: its push constants are {1} bytes, but the pass sends {2}.", shaderName, program.PushConstantSize, pushConstantSize);
            return false;
        }

        for (const ShaderBinding& binding : program.Bindings)
        {
            const bool known = binding.Set == 0
                && ((binding.Binding == SceneBinding && binding.Kind == ShaderBindingKind::UniformBuffer)
                    || (binding.Binding == TextureBinding && binding.Kind == ShaderBindingKind::SampledTexture)
                    || (binding.Binding == SamplerBinding && binding.Kind == ShaderBindingKind::Sampler)
                    || (binding.Binding == ComparisonSamplerBinding && binding.Kind == ShaderBindingKind::Sampler));
            if (!known)
            {
                LX_CORE_ERROR("Shader {0}: parameter '{1}' is not one of the shared bindings. Import Scene or Textures, and pass everything else as push constants.", shaderName, binding.Name);
                return false;
            }
        }
        return true;
    }

    void SharedBindings::Bind(vk::CommandBuffer commands) const
    {
        commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, m_Set, nullptr);
    }
}
