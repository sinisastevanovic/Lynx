#pragma once

#include <vulkan/vulkan.hpp>

// The formats of the pictures a frame is drawn into. A pipeline has to name the format of its targets,
// so the passes and the code that creates the pictures read them from here.
namespace Lynx::RenderFormats
{
    // The scene before post-processing, with brightness above 1.
    constexpr vk::Format SceneColor = vk::Format::eR16G16B16A16Sfloat;
    constexpr vk::Format SceneDepth = vk::Format::eD32Sfloat;

    // Next to the colour of the scene: for every pixel, how far what it shows moved on screen since the
    // last frame, in texture coordinates. Half floats tell a hundredth of a pixel apart at any picture size.
    constexpr vk::Format SceneMotion = vk::Format::eR16G16Sfloat;

    // Next to the colour of the scene: for every pixel, how much of it is covered by what is see-through
    // and has no motion of its own in the picture of motion, from 0 to 1. Known elsewhere as the reactive mask.
    constexpr vk::Format SceneReactive = vk::Format::eR8Unorm;
    constexpr vk::Format ShadowMap = vk::Format::eD32Sfloat;

    // The scene's depth in ever smaller sizes, which culling reads. A depth format cannot be drawn into
    // by a fragment shader's output, so it is a colour format with the same precision.
    constexpr vk::Format DepthPyramid = vk::Format::eR32Sfloat;

    // The tables the sky is read from, see Atmosphere.h. They hold light, which can be brighter than 1.
    constexpr vk::Format AtmosphereTable = vk::Format::eR16G16B16A16Sfloat;

    // Next to the colour of the scene in the editor: for every pixel, the ID of the entity it shows.
    constexpr vk::Format EntityIds = vk::Format::eR32Sint;

    // The finished picture: the window's images and the editor's viewport.
    constexpr vk::Format Output = vk::Format::eB8G8R8A8Unorm;
}
