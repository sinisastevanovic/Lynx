#pragma once

#include "Lynx/Renderer/Shader/ShaderProgram.h"

#include <vulkan/vulkan.hpp>

#include <vector>

namespace Lynx
{
    enum class BlendMode
    {
        // The new colour replaces the old one.
        Opaque,
        // The new colour covers the old one by its alpha.
        Alpha,
        // The new colour is added to the old one.
        Additive
    };

    struct VertexAttribute
    {
        vk::Format Format = vk::Format::eUndefined;
        uint32_t Offset = 0;
    };

    // Everything that makes one graphics pipeline, with the values most passes want as defaults.
    // Viewport and scissor are not part of it: they follow the size of the target and are set when drawing.
    struct GraphicsPipelineDesc
    {
        const ShaderProgram* Program = nullptr;
        vk::PipelineLayout Layout;

        // False leaves the program's fragment stage out. For drawing depth only where nothing is decided
        // per pixel: the GPU then runs no code at all for a pixel.
        bool UseFragmentStage = true;

        // The attributes of one vertex buffer, in the order of the shader's inputs. Empty for a pass
        // whose vertex shader makes its vertices from the vertex index alone.
        std::vector<VertexAttribute> VertexAttributes;
        uint32_t VertexStride = 0;

        vk::PrimitiveTopology Topology = vk::PrimitiveTopology::eTriangleList;
        vk::CullModeFlags CullMode = vk::CullModeFlagBits::eNone;

        // Which winding is the front. Only matters with a cull mode. The engine's meshes are counter-clockwise.
        vk::FrontFace FrontFace = vk::FrontFace::eCounterClockwise;

        // Pushes what is drawn away from the camera: by a fixed amount, and by more the steeper the surface is.
        // Used for shadow maps, where a surface would otherwise shadow itself.
        float DepthBias = 0.0f;
        float DepthBiasSlope = 0.0f;
        BlendMode Blend = BlendMode::Opaque;

        // Undefined for a pass that draws depth only.
        vk::Format ColorFormat = vk::Format::eUndefined;

        // A second target that receives whole numbers and is never blended: the entity IDs the editor picks by.
        // Undefined for none.
        vk::Format IdFormat = vk::Format::eUndefined;

        // Undefined for a pass without a depth buffer.
        vk::Format DepthFormat = vk::Format::eUndefined;
        bool DepthTest = false;
        bool DepthWrite = false;

        // The scene's depth is 1 at the near plane and smaller with distance, so "in front" means greater.
        vk::CompareOp DepthCompare = vk::CompareOp::eGreater;
    };

    // Returns a null handle, after logging why, if the program lacks a stage the pipeline uses.
    vk::Pipeline CreateGraphicsPipeline(vk::Device device, const GraphicsPipelineDesc& desc);
}
