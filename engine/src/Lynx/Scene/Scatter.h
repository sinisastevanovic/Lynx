#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Lynx
{
    class StaticMesh;

    // One scattered thing: a tree, a rock, a tuft of grass. A forest has hundreds of thousands of them,
    // so one is as small as it can be: where it stands, how far it is turned about the upward axis, how
    // large it is, and which kind of thing it is. It always stands upright and is scaled alike in every
    // direction. Has to match ScatterInstance in Scene.slang, which builds the same matrix.
    //
    // They are not entities: nothing but the renderer knows a single one of them.
    struct ScatterInstance
    {
        // How many kinds a list of instances can tell apart.
        static constexpr uint32_t KindCount = 4095;

        // The kind of an instance that is not drawn.
        static constexpr uint32_t NoKind = 4095;

        glm::vec3 Position{ 0.0f };

        // The turn in the lowest 12 bits, the size in the 8 above them, the kind in the 12 above those.
        uint32_t Packed = NoKind << KindShift;

        // turns is the turn about the upward axis in whole turns, of which only the part after the
        // point counts. A size is kept to within about one percent, from a quarter to four times.
        static ScatterInstance At(const glm::vec3& position, float turns, float size, uint32_t kind)
        {
            const float partOfTurn = turns - std::floor(turns);
            const uint32_t turn = static_cast<uint32_t>(partOfTurn * static_cast<float>(TurnSteps)) % TurnSteps;
            const float sizeSteps = std::round(SizeMiddle + SizeStepsPerDoubling * std::log2(std::max(size, 0.001f)));
            const uint32_t storedSize = static_cast<uint32_t>(std::clamp(sizeSteps, 0.0f, 255.0f));
            return { position, turn | (storedSize << SizeShift) | (std::min(kind, NoKind) << KindShift) };
        }

        // The turn about the upward axis, in whole turns from 0 to 1.
        float Turns() const { return static_cast<float>(Packed & (TurnSteps - 1)) / static_cast<float>(TurnSteps); }

        float Size() const { return std::exp2((static_cast<float>((Packed >> SizeShift) & 0xFF) - SizeMiddle) / SizeStepsPerDoubling); }

        uint32_t Kind() const { return Packed >> KindShift; }

        ScatterInstance WithKind(uint32_t kind) const
        {
            return { Position, (Packed & ((1u << KindShift) - 1)) | (std::min(kind, NoKind) << KindShift) };
        }

    private:
        static constexpr uint32_t TurnSteps = 4096;
        static constexpr uint32_t SizeShift = 12;
        static constexpr uint32_t KindShift = 20;
        static constexpr float SizeMiddle = 128.0f;
        static constexpr float SizeStepsPerDoubling = 64.0f;
    };
    static_assert(sizeof(ScatterInstance) == 16);

    // Things scattered over a scene, which the renderer draws. An instance names its kind by its number
    // in the list of kinds, and a kind is a mesh, drawn with the mesh's own materials. Surfaces that
    // are see-through are not drawn.
    //
    // It lives in a scene's registry like a component, but is not one that is saved, shown or copied
    // into a played scene: whoever scatters makes it anew from its rules. Replace it as a whole
    // (registry.emplace_or_replace), which is how the renderer learns that it changed.
    struct ScatterComponent
    {
        std::vector<std::shared_ptr<StaticMesh>> Kinds;
        std::vector<ScatterInstance> Instances;
    };
}
