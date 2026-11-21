#pragma once

#include "Lynx/Renderer/Shader/ShaderProgram.h"

#include <vulkan/vulkan.hpp>

namespace Lynx
{
    // The caller destroys the module, which is allowed as soon as the pipeline made from it exists.
    vk::ShaderModule CreateShaderModule(vk::Device device, const ShaderStageCode& code);
}
