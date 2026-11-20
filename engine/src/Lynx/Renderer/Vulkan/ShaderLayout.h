#pragma once

#include "Lynx/Renderer/Shader/ShaderProgram.h"

#include <vulkan/vulkan.hpp>

#include <optional>
#include <string_view>
#include <vector>

// Translates what a compiled shader program expects into the objects Vulkan wants,
// so that a pass never spells out a binding number or a descriptor type by hand.
namespace Lynx
{
    vk::ShaderStageFlags ToVulkan(ShaderStageMask stages);
    vk::DescriptorType ToVulkan(ShaderBindingKind kind);

    // Every binding is visible to all stages of the program.
    vk::DescriptorSetLayout CreateSetLayout(vk::Device device, const ShaderProgram& program, uint32_t set);

    // A layout with the given sets and the push constants the program declares.
    vk::PipelineLayout CreatePipelineLayout(vk::Device device, const ShaderProgram& program, vk::ArrayProxy<const vk::DescriptorSetLayout> setLayouts);

    // The caller destroys the module, which is allowed as soon as the pipeline made from it exists.
    vk::ShaderModule CreateShaderModule(vk::Device device, const ShaderStageCode& code);

    // The binding a parameter of the shader was given, by the name it has in the shader source.
    std::optional<uint32_t> FindBinding(const ShaderProgram& program, std::string_view name);
}
