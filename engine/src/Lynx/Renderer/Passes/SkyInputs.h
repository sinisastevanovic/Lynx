#pragma once
#include "Lynx/Renderer/RenderPass.h"

#include <cmath>

namespace Lynx
{
    // Everything the light of the sky on the scene depends on. The passes that work that light out
    // keep the inputs they last ran with and only run again when these differ: the sun stands still
    // in most frames, and their results stay on the GPU.
    struct SkyInputs
    {
        AtmosphereSettings Atmosphere;
        glm::vec3 SunDirection{ 0.0f };
        glm::vec3 SunBrightness{ 0.0f };

        // The camera's height above the ground, in steps of a quarter doubling. The sky changes little
        // with height near the ground, and a camera that moves would otherwise count as a new sky every frame.
        int32_t HeightStep = 0;

        bool operator==(const SkyInputs&) const = default;
    };

    inline SkyInputs SkyInputsOf(const RenderData& renderData)
    {
        SkyInputs inputs;
        inputs.Atmosphere = renderData.Atmosphere;
        inputs.SunDirection = renderData.LightDirection;
        inputs.SunBrightness = renderData.LightColor * renderData.LightIntensity;
        inputs.HeightStep = static_cast<int32_t>(std::floor(std::log2(std::max(renderData.CameraPosition.y, 1.0f)) * 4.0f));
        return inputs;
    }
}
