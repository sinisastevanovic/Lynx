#include "GraphicsPipeline.h"

#include "ShaderLayout.h"
#include "Lynx/Log.h"

namespace Lynx
{
    namespace
    {
        vk::PipelineColorBlendAttachmentState BlendState(BlendMode mode)
        {
            vk::PipelineColorBlendAttachmentState state;
            state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

            if (mode == BlendMode::Opaque)
                return state;

            state.blendEnable = VK_TRUE;
            state.colorBlendOp = vk::BlendOp::eAdd;
            state.alphaBlendOp = vk::BlendOp::eAdd;
            state.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            state.dstColorBlendFactor = mode == BlendMode::Alpha ? vk::BlendFactor::eOneMinusSrcAlpha : vk::BlendFactor::eOne;
            state.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            state.dstAlphaBlendFactor = mode == BlendMode::Alpha ? vk::BlendFactor::eOneMinusSrcAlpha : vk::BlendFactor::eOne;
            return state;
        }
    }

    vk::Pipeline CreateGraphicsPipeline(vk::Device device, const GraphicsPipelineDesc& desc)
    {
        const ShaderStageCode* vertexCode = desc.Program->FindStage(ShaderStage::Vertex);
        const ShaderStageCode* fragmentCode = desc.UseFragmentStage ? desc.Program->FindStage(ShaderStage::Fragment) : nullptr;
        if (!vertexCode || (desc.UseFragmentStage && !fragmentCode))
        {
            LX_CORE_ERROR("A graphics pipeline needs a shader with a vertex and a fragment stage.");
            return nullptr;
        }

        // A pipeline keeps nothing of the modules it was made from, so they only live until it exists.
        const vk::ShaderModule vertexModule = CreateShaderModule(device, *vertexCode);
        const vk::ShaderModule fragmentModule = fragmentCode ? CreateShaderModule(device, *fragmentCode) : vk::ShaderModule();
        const vk::PipelineShaderStageCreateInfo stages[] = {
            { {}, vk::ShaderStageFlagBits::eVertex, vertexModule, "main" },
            { {}, vk::ShaderStageFlagBits::eFragment, fragmentModule, "main" },
        };
        const uint32_t stageCount = fragmentCode ? 2 : 1;

        const vk::VertexInputBindingDescription vertexBinding(0, desc.VertexStride, vk::VertexInputRate::eVertex);
        std::vector<vk::VertexInputAttributeDescription> attributes;
        for (uint32_t location = 0; location < desc.VertexAttributes.size(); ++location)
            attributes.emplace_back(location, 0, desc.VertexAttributes[location].Format, desc.VertexAttributes[location].Offset);

        vk::PipelineVertexInputStateCreateInfo vertexInput;
        if (!attributes.empty())
        {
            vertexInput.setVertexBindingDescriptions(vertexBinding);
            vertexInput.setVertexAttributeDescriptions(attributes);
        }

        const vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, desc.Topology);

        const vk::PipelineViewportStateCreateInfo viewport({}, 1, nullptr, 1, nullptr);
        std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        if (desc.DynamicCullMode)
            dynamicStates.push_back(vk::DynamicState::eCullMode);
        const vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

        vk::PipelineRasterizationStateCreateInfo rasterization;
        rasterization.polygonMode = desc.Wireframe ? vk::PolygonMode::eLine : vk::PolygonMode::eFill;
        rasterization.cullMode = desc.CullMode;
        rasterization.frontFace = desc.FrontFace;
        rasterization.lineWidth = 1.0f;
        rasterization.depthBiasEnable = desc.DepthBias != 0.0f || desc.DepthBiasSlope != 0.0f;
        rasterization.depthBiasConstantFactor = desc.DepthBias;
        rasterization.depthBiasSlopeFactor = desc.DepthBiasSlope;

        const vk::PipelineMultisampleStateCreateInfo multisample({}, vk::SampleCountFlagBits::e1);

        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        depthStencil.depthTestEnable = desc.DepthTest;
        depthStencil.depthWriteEnable = desc.DepthWrite;
        depthStencil.depthCompareOp = desc.DepthCompare;

        // The colour target first, then the motion, the see-through mask and the IDs, each only if the pass has it.
        std::vector<vk::Format> colorFormats;
        std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments;
        if (desc.ColorFormat != vk::Format::eUndefined)
        {
            colorFormats.push_back(desc.ColorFormat);
            blendAttachments.push_back(BlendState(desc.Blend));
        }
        if (desc.MotionFormat != vk::Format::eUndefined)
        {
            vk::PipelineColorBlendAttachmentState motion = BlendState(BlendMode::Opaque);
            if (desc.Blend != BlendMode::Opaque)
                motion.colorWriteMask = {};
            colorFormats.push_back(desc.MotionFormat);
            blendAttachments.push_back(motion);
        }
        if (desc.ReactiveFormat != vk::Format::eUndefined)
        {
            // What a see-through surface covers adds up: its own share, plus what was there of the rest.
            vk::PipelineColorBlendAttachmentState reactive;
            if (desc.Blend != BlendMode::Opaque)
            {
                reactive.colorWriteMask = vk::ColorComponentFlagBits::eR;
                reactive.blendEnable = VK_TRUE;
                reactive.colorBlendOp = vk::BlendOp::eAdd;
                reactive.alphaBlendOp = vk::BlendOp::eAdd;
                reactive.srcColorBlendFactor = vk::BlendFactor::eOne;
                reactive.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
                reactive.srcAlphaBlendFactor = vk::BlendFactor::eOne;
                reactive.dstAlphaBlendFactor = vk::BlendFactor::eZero;
            }
            colorFormats.push_back(desc.ReactiveFormat);
            blendAttachments.push_back(reactive);
        }
        if (desc.IdFormat != vk::Format::eUndefined)
        {
            colorFormats.push_back(desc.IdFormat);
            blendAttachments.push_back(BlendState(BlendMode::Opaque));
        }
        vk::PipelineColorBlendStateCreateInfo blend;
        blend.setAttachments(blendAttachments);

        // No render pass object: the formats are all the pipeline needs to know about its targets.
        vk::PipelineRenderingCreateInfo rendering;
        rendering.setColorAttachmentFormats(colorFormats);
        rendering.depthAttachmentFormat = desc.DepthFormat;

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.pNext = &rendering;
        pipelineInfo.stageCount = stageCount;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = desc.Layout;

        const vk::Pipeline pipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;

        device.destroyShaderModule(vertexModule);
        device.destroyShaderModule(fragmentModule);
        return pipeline;
    }
}
