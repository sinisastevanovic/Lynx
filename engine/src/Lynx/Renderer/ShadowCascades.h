#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace Lynx
{
    // The sun's shadow is kept in several maps, the cascades. Each covers a stretch of what the camera
    // sees, the first the nearest: a small area in fine detail close by, large areas more coarsely far away.
    // One map over everything would be too coarse close by or would not reach far enough.
    constexpr uint32_t ShadowCascadeCount = 4;

    // The width and height of the map of one cascade, in texels.
    constexpr uint32_t ShadowCascadeResolution = 2048;

    // A place takes its shadow from the first cascade whose map it lies in. In the outer part of a map,
    // this share of the way from its middle to its edge, the shadow is mixed with that of the next
    // cascade, so that no line shows where the detail changes. Has to match Scene.slang.
    constexpr float ShadowCascadeBlend = 0.1f;

    struct ShadowSettings
    {
        // How far from the camera shadows are drawn, in the scene's units.
        float Distance = 250.0f;

        // How the distance is shared out among the cascades, from 0 to 1. At 0 every cascade is as deep as
        // the others. At 1 each is as many times deeper than the one before, which gives the near ones the
        // detail and is too much of it right in front of the camera. The usual value lies close to 1.
        float SplitWeight = 0.85f;

        // Something smaller than this many texels of a cascade's map, measured by the radius of the
        // sphere around it, casts no shadow into that cascade: its shadow would be a blur of a few
        // texels, and a far cascade would otherwise draw every small thing in a wide area for it.
        // So small things lose their shadow some way from the camera, large things keep it to the end.
        float SmallestCasterInTexels = 4.0f;

        // How far towards the sun and away from it a cascade reaches beyond the stretch of the view it is
        // fitted to: for what stands outside and casts its shadow in, and for what lies outside and receives it.
        float DepthRange = 500.0f;
    };

    struct ShadowCascade
    {
        // From the world to what the sun sees of this cascade, as GLM makes it: +Y up, depth 0 at the near plane.
        glm::mat4 ViewProjection{ 1.0f };

        // The world as the sun sees it, turned but not moved. The same for every cascade.
        glm::mat4 SunView{ 1.0f };

        // The square of ground the map covers, in that view: its middle, and half its width.
        glm::vec2 Centre{ 0.0f };
        float Radius = 0.0f;

        // The size of one texel of its map in the world.
        float TexelSize = 0.0f;
    };

    // Fits the cascades to what a camera sees, for a sun that shines in the given direction.
    //
    // Each cascade covers the sphere around its stretch of the view, not the stretch itself, and moves in
    // steps of whole texels. So its map shows the world in the same way whichever way the camera turns and
    // wherever it goes, and the edges of shadows do not crawl ("Stable Cascaded Shadow Maps", Valient).
    std::array<ShadowCascade, ShadowCascadeCount> FitShadowCascades(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& sunDirection, const ShadowSettings& settings);
}
