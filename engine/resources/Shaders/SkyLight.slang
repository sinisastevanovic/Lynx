// Works out the light that the sky and the planet's ground send onto a surface, for every way the
// surface can face, and stores it as nine numbers per colour (spherical harmonics, after "An Efficient
// Representation for Irradiance Environment Maps", Ramamoorthi and Hanrahan, 2001). Light gathered
// from half the sky changes slowly with the direction, so nine numbers hold it almost exactly, and a
// pixel gets its light from the sky with a handful of multiplications. See skyIrradiance in Atmosphere.slang.
//
// Runs once per frame as a single thread: it looks at the sky in a few hundred directions, which the
// sky view table answers.

import Atmosphere;
import Scene;

static const float pi = 3.14159265359;

// Evenly spread over the sphere. The sky has no detail finer than nine numbers can keep, the sun's
// disc is not part of it, so a few hundred are plenty.
static const uint directionCount = 512;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain()
{
    float3 sum[skyIrradianceCount];
    for (uint index = 0; index < skyIrradianceCount; ++index)
        sum[index] = float3(0.0, 0.0, 0.0);

    for (uint point = 0; point < directionCount; ++point)
    {
        // A spiral from the top of the sphere to the bottom, turned on by the golden angle at every step.
        float upwards = 1.0 - 2.0 * (float(point) + 0.5) / float(directionCount);
        float sideways = sqrt(saturate(1.0 - upwards * upwards));
        float around = float(point) * 2.39996323;
        float3 direction = float3(cos(around) * sideways, upwards, sin(around) * sideways);

        float3 light = skyLight(direction) + groundLight(direction);

        float basis[skyIrradianceCount];
        skyIrradianceBasis(direction, basis);
        for (uint index = 0; index < skyIrradianceCount; ++index)
            sum[index] += light * basis[index];
    }

    // The light from each direction, as nine numbers, becomes the light a surface gathers over the
    // half of the sphere it faces by scaling the numbers: by pi for the first, two thirds of pi for
    // the next three, a quarter of pi for the last five.
    static const float gathered[skyIrradianceCount] = { pi, 2.0 * pi / 3.0, 2.0 * pi / 3.0, 2.0 * pi / 3.0, pi / 4.0, pi / 4.0, pi / 4.0, pi / 4.0, pi / 4.0 };
    float perDirection = 4.0 * pi / float(directionCount);
    for (uint index = 0; index < skyIrradianceCount; ++index)
        scene.skyIrradiance[index] = float4(sum[index] * (perDirection * gathered[index]), 0.0);
}
