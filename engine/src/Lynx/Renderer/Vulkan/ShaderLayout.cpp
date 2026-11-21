#include "ShaderLayout.h"

namespace Lynx
{
    vk::ShaderModule CreateShaderModule(vk::Device device, const ShaderStageCode& code)
    {
        return device.createShaderModule(vk::ShaderModuleCreateInfo({}, code.Spirv));
    }
}
