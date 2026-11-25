#pragma once

#include <glm/glm.hpp>

namespace Lynx
{
    // The air above the ground of a scene, which colours the sky and the sun's light. An entity carries
    // it as its "SkyAtmosphere" component, and the first enabled one counts. A scene without one has the
    // Earth's air, which is what the defaults are, with the values of Bruneton and Hillaire.
    //
    // Lengths are in kilometres. How much light the air scatters or swallows is given per thousand
    // kilometres of the way through it at ground level, which keeps the numbers readable.
    // The renderer works the sky out from this, see Renderer/Atmosphere.h.
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
        // Also the colour of the bare ground that is drawn where the scene has none.
        glm::vec3 GroundAlbedo{ 0.3f };

        bool operator==(const AtmosphereSettings&) const = default;
    };
}
