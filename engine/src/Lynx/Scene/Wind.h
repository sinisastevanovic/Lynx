#pragma once

namespace Lynx
{
    // The wind over a scene, which moves trees and other plants. An entity carries this as its "Wind"
    // component, and the first enabled one counts. A scene without one has the defaults, a light breeze.
    //
    // The wind is one direction and one speed for the whole scene. What moves in it is worked out in
    // the vertex shaders, see windOn in Mesh.slang: nothing is simulated and nothing is stored.
    struct WindSettings
    {
        // Where the wind blows towards, in degrees about the upward axis: 0 along +X, 90 along +Z.
        float Direction = 0.0f;

        // In metres a second. As a guide, the Beaufort scale: 2 is a light breeze that moves leaves,
        // 6 a breeze that moves small branches, 12 a strong breeze that moves large branches, and 20 a
        // gale in which whole trees are in motion. Zero is still air.
        float Speed = 3.0f;

        // How unevenly it blows: 0 is the same everywhere and always, 1 goes from still air to twice
        // the speed in waves that travel with the wind.
        float Gustiness = 0.6f;

        bool operator==(const WindSettings&) const = default;
    };
}
