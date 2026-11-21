#pragma once

#include <vulkan/vulkan.hpp>

// The formats of the pictures a frame is drawn into. A pipeline has to name the format of its targets,
// so the passes and the code that creates the pictures read them from here.
namespace Lynx::RenderFormats
{
    // The scene before post-processing, with brightness above 1.
    constexpr vk::Format SceneColor = vk::Format::eR16G16B16A16Sfloat;
    constexpr vk::Format SceneDepth = vk::Format::eD32Sfloat;
    constexpr vk::Format ShadowMap = vk::Format::eD32Sfloat;

    // The finished picture: the window's images and the editor's viewport.
    constexpr vk::Format Output = vk::Format::eB8G8R8A8Unorm;
}
