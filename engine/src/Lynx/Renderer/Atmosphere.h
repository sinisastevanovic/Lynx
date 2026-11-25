#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace Lynx
{
    // The air above the ground, which colours the sky and the sun's light. The defaults are the Earth's,
    // with the values of Bruneton and Hillaire. Lengths are in kilometres. How much light the air
    // scatters or swallows is given per thousand kilometres of the way through it at ground level,
    // which keeps the numbers readable.
    struct AtmosphereSettings
    {
        float PlanetRadius = 6360.0f;

        // How far above the ground the air ends.
        float Height = 100.0f;

        // The air's molecules scatter blue light most, which is the blue of the sky and the red of a
        // low sun. They thin out with height: at RayleighHeight there are 1/e of them left.
        glm::vec3 RayleighScattering{ 5.802f, 13.558f, 33.1f };
        float RayleighHeight = 8.0f;

        // Dust and droplets scatter all colours alike and mostly onwards, which is the white glare
        // around the sun and the haze at the horizon. More of them makes a hazier day.
        float MieScattering = 3.996f;
        float MieAbsorption = 4.4f;
        float MieHeight = 1.2f;

        // How strongly dust scatters onwards, from 0 for evenly to nearly 1 for straight on.
        float MieAnisotropy = 0.8f;

        // Ozone swallows some green and red high up, which keeps the sky blue at dusk.
        glm::vec3 OzoneAbsorption{ 0.650f, 1.881f, 0.085f };

        // The colour of the ground as the air sees it from above: its light comes back into the sky.
        glm::vec3 GroundAlbedo{ 0.3f };

        bool operator==(const AtmosphereSettings&) const = default;
    };

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

        // The camera stays above the ground: from below it there is no sky.
        const float camera = settings.PlanetRadius + glm::max(cameraHeight, 1.0f) * kilometresPerUnit;

        AtmosphereData data;
        data.Rayleigh = glm::vec4(settings.RayleighScattering * perKilometre, 1.0f / settings.RayleighHeight);
        data.Mie = glm::vec4(settings.MieScattering * perKilometre, settings.MieAbsorption * perKilometre, 1.0f / settings.MieHeight, settings.MieAnisotropy);
        data.Ozone = glm::vec4(settings.OzoneAbsorption * perKilometre, 0.0f);
        data.GroundAlbedo = glm::vec4(settings.GroundAlbedo, 0.0f);
        data.Radii = glm::vec4(settings.PlanetRadius, settings.PlanetRadius + settings.Height, glm::min(camera, settings.PlanetRadius + settings.Height - 0.001f), 0.0f);
        data.Tables = tables;
        return data;
    }
}
