#pragma once

#include "Lynx/Scene/Atmosphere.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace Lynx
{
    // The pictures the sky is read from, see Atmosphere.slang. The sizes have to match the shader.
    enum class AtmosphereTable : uint32_t { Transmittance, MultiScattering, SkyView, Count };

    constexpr std::array<glm::uvec2, static_cast<size_t>(AtmosphereTable::Count)> AtmosphereTableSizes = {
        glm::uvec2(256, 64), glm::uvec2(32, 32), glm::uvec2(192, 108)
    };

    // The cube shiny surfaces mirror the sky from: the length of its edge, and how many sizes it has,
    // down to the one for a fully rough surface. The number of sizes has to match Atmosphere.slang.
    constexpr uint32_t SkyReflectionSize = 64;
    constexpr uint32_t SkyReflectionMips = 6;

    // The atmosphere as shaders get it, part of the scene constants: in kilometres and per kilometre.
    // Has to match AtmosphereConstants in Scene.slang.
    struct AtmosphereData
    {
        // Scattering in xyz, one over the height in w.
        glm::vec4 Rayleigh;

        // Scattering, absorption, one over the height, anisotropy.
        glm::vec4 Mie;
        glm::vec4 Ozone;
        glm::vec4 GroundAlbedo;

        // From the planet's middle: the ground, the top of the air, the camera.
        glm::vec4 Radii;

        // Where the texture table has the three tables, and the sampler table the sampler they are read with.
        glm::uvec4 Tables;
    };

    // cameraHeight is in the scene's units, metres above the ground.
    inline AtmosphereData ToAtmosphereData(const AtmosphereSettings& settings, float cameraHeight, const glm::uvec4& tables)
    {
        constexpr float perKilometre = 0.001f;
        constexpr float kilometresPerUnit = 0.001f;

        // The settings can be typed in by hand. A planet or a layer of air of no size would divide by
        // zero, and dust that scatters straight on is a single point of infinite brightness.
        constexpr float smallest = 0.01f;
        const float planet = glm::max(settings.PlanetRadius, 1.0f);
        const float height = glm::max(settings.Height, 1.0f);

        // The camera stays above the ground: from below it there is no sky.
        const float camera = planet + glm::max(cameraHeight, 1.0f) * kilometresPerUnit;

        AtmosphereData data;
        data.Rayleigh = glm::vec4(glm::max(settings.RayleighScattering, 0.0f) * perKilometre, 1.0f / glm::max(settings.RayleighHeight, smallest));
        data.Mie = glm::vec4(glm::max(settings.MieScattering, 0.0f) * perKilometre, glm::max(settings.MieAbsorption, 0.0f) * perKilometre,
            1.0f / glm::max(settings.MieHeight, smallest), glm::clamp(settings.MieAnisotropy, 0.0f, 0.99f));
        data.Ozone = glm::vec4(glm::max(settings.OzoneAbsorption, 0.0f) * perKilometre, 0.0f);
        data.GroundAlbedo = glm::vec4(glm::clamp(settings.GroundAlbedo, 0.0f, 1.0f), 0.0f);
        data.Radii = glm::vec4(planet, planet + height, glm::min(camera, planet + height - 0.001f), 0.0f);
        data.Tables = tables;
        return data;
    }
}
