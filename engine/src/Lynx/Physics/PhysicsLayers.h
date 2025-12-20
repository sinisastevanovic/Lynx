#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace Lynx
{
    namespace Layers
    {
        static constexpr JPH::ObjectLayer STATIC = 0;
        static constexpr JPH::ObjectLayer MOVABLE = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer STATIC(0);
        static constexpr JPH::BroadPhaseLayer MOVABLE(1);
        static constexpr JPH::uint NUM_LAYERS(2);
    }
}
