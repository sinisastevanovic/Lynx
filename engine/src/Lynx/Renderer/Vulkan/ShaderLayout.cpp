#include "ShaderLayout.h"

namespace Lynx
{
    vk::ShaderStageFlags ToVulkan(ShaderStageMask stages)
    {
        vk::ShaderStageFlags flags;
        if (stages & ToMask(ShaderStage::Vertex))
            flags |= vk::ShaderStageFlagBits::eVertex;
        if (stages & ToMask(ShaderStage::Fragment))
            flags |= vk::ShaderStageFlagBits::eFragment;
        if (stages & ToMask(ShaderStage::Compute))
            flags |= vk::ShaderStageFlagBits::eCompute;
        return flags;
    }

    vk::DescriptorType ToVulkan(ShaderBindingKind kind)
    {
        switch (kind)
        {
            case ShaderBindingKind::SampledTexture:     return vk::DescriptorType::eSampledImage;
            case ShaderBindingKind::Sampler:            return vk::DescriptorType::eSampler;
            case ShaderBindingKind::TextureWithSampler: return vk::DescriptorType::eCombinedImageSampler;
            case ShaderBindingKind::StorageTexture:     return vk::DescriptorType::eStorageImage;
            case ShaderBindingKind::UniformBuffer:      return vk::DescriptorType::eUniformBuffer;
            case ShaderBindingKind::StorageBuffer:      return vk::DescriptorType::eStorageBuffer;
        }
        return vk::DescriptorType::eSampledImage;
    }

    vk::DescriptorSetLayout CreateSetLayout(vk::Device device, const ShaderProgram& program, uint32_t set)
    {
        const vk::ShaderStageFlags stages = ToVulkan(program.StageMask());

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (const ShaderBinding& binding : program.Bindings)
        {
            if (binding.Set == set)
                bindings.emplace_back(binding.Binding, ToVulkan(binding.Kind), binding.Count, stages);
        }

        return device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, bindings));
    }

    vk::PipelineLayout CreatePipelineLayout(vk::Device device, const ShaderProgram& program, vk::ArrayProxy<const vk::DescriptorSetLayout> setLayouts)
    {
        vk::PipelineLayoutCreateInfo layoutInfo;
        layoutInfo.setLayoutCount = setLayouts.size();
        layoutInfo.pSetLayouts = setLayouts.data();

        const vk::PushConstantRange pushConstants(ToVulkan(program.StageMask()), 0, program.PushConstantSize);
        if (program.PushConstantSize > 0)
        {
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &pushConstants;
        }

        return device.createPipelineLayout(layoutInfo);
    }

    vk::ShaderModule CreateShaderModule(vk::Device device, const ShaderStageCode& code)
    {
        return device.createShaderModule(vk::ShaderModuleCreateInfo({}, code.Spirv));
    }

    std::optional<uint32_t> FindBinding(const ShaderProgram& program, std::string_view name)
    {
        for (const ShaderBinding& binding : program.Bindings)
        {
            if (binding.Name == name)
                return binding.Binding;
        }
        return std::nullopt;
    }
}
