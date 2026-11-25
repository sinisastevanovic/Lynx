#pragma once

#include <array>
#include <cstdint>

namespace Lynx
{
    // What a view of a scene shows of its meshes. Everything but Lit shows one thing about the picture
    // instead of the picture, for finding out how it comes about.
    // Has to match the numbers in StandardShading.slang.
    enum class ViewMode : uint32_t
    {
        // The finished picture.
        Lit,

        // The edges of the triangles that are drawn, of the surfaces that are not hidden.
        Wireframe,

        // The colour of the surfaces, without any light.
        Unlit,

        // The light on the surfaces, as if all of them were the same grey.
        LightingOnly,

        // Which way the surfaces face, after their normal maps.
        Normals,

        // The level of detail each mesh is drawn in: green for the finest, towards red for coarser ones.
        LevelOfDetail,

        // Which cascade of the sun's shadow each surface is in, nearest first: red, green, blue, yellow.
        ShadowCascades,

        // How far every pixel moved on screen since the last frame: grey for not at all, redder to the
        // right, greener downwards. Shown by the composite pass, from the picture the scene passes wrote.
        Motion,

        // How anti-aliasing over time worked the frame out, see TemporalPass. How much the frame counts
        // against the history: black for hardly at all, white for all there is.
        TemporalBlend,

        // Where the history was dropped: blue where it lies off the screen, red where it showed another surface.
        TemporalRejected,

        // How far the history was pulled to fit the colours around the pixel: what would have been a trail.
        TemporalClipped,
    };

    // The modes by their numbers, as a list to choose from.
    inline constexpr std::array<const char*, 11> ViewModeNames = { "Lit", "Wireframe", "Unlit", "Lighting only", "Normals", "Level of detail", "Shadow cascades", "Motion",
        "TAA: share of the frame", "TAA: history dropped", "TAA: history pulled in" };

    // True for a mode whose colours mean something as they are. They are shown without bloom and tone
    // mapping, which would change them.
    constexpr bool ShowsRawColours(ViewMode mode)
    {
        return mode == ViewMode::Wireframe || mode == ViewMode::Unlit || mode == ViewMode::Normals || mode == ViewMode::LevelOfDetail
            || mode == ViewMode::ShadowCascades;
    }

    // How one view of a scene is shown. It belongs to the view, not to the renderer: whoever draws a
    // scene says it with the scene (ExtractedScene), so two views can show the same scene differently.
    struct ViewSettings
    {
        ViewMode Mode = ViewMode::Lit;

        // False draws materials with cut-outs whole, as the triangles they are made of.
        bool ShowCutOuts = true;
    };
}
