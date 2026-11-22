#include "ComputePipeline.h"

#include "ShaderLayout.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/Shader/ShaderProgram.h"

namespace Lynx
{
    vk::Pipeline CreateComputePipeline(vk::Device device, const ShaderProgram& program, vk::PipelineLayout layout)
    {
        const ShaderStageCode* code = program.FindStage(ShaderStage::Compute);
        if (!code)
        {
            LX_CORE_ERROR("A compute pipeline needs a shader with a compute stage.");
            return nullptr;
        }

        // A pipeline keeps nothing of the module it was made from, so it only lives until the pipeline exists.
        const vk::ShaderModule module = CreateShaderModule(device, *code);
        const vk::PipelineShaderStageCreateInfo stage({}, vk::ShaderStageFlagBits::eCompute, module, "main");

        const auto created = device.createComputePipeline(nullptr, vk::ComputePipelineCreateInfo({}, stage, layout));
        device.destroyShaderModule(module);

        if (created.result != vk::Result::eSuccess)
        {
            LX_CORE_ERROR("Could not create a compute pipeline: {0}", vk::to_string(created.result));
            return nullptr;
        }
        return created.value;
    }
}
