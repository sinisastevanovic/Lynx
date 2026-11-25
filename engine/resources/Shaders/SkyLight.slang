// Works out the light that the sky and the planet's ground send onto a surface, for every way the
// surface can face, and stores it as nine numbers per colour (spherical harmonics, after "An Efficient
// Representation for Irradiance Environment Maps", Ramamoorthi and Hanrahan, 2001). Light gathered
// from half the sky changes slowly with the direction, so nine numbers hold it almost exactly, and a
// pixel gets its light from the sky with a handful of multiplications. See skyIrradiance in Atmosphere.slang.
//
// Runs as one group of threads, in a frame in which the sky has changed. Each looks at the sky in a few directions, which the
// sky view table answers, and the first adds up what all of them found. One thread doing all of it
// alone took 0.2 ms, twenty times as long.

import Atmosphere;
import Scene;

static const float pi = 3.14159265359;

// Has to match the numthreads below.
static const uint threadCount = 64;

// Evenly spread over the sphere, in all 512. The sky has no detail finer than nine numbers can keep,
// and the sun's disc is not part of it, so that is plenty.
static const uint directionsPerThread = 8;
static const uint directionCount = threadCount * directionsPerThread;

// What each thread found, where the first can read it.
static groupshared float3 found[threadCount * skyIrradianceCount];

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 thread : SV_GroupThreadID)
{
    float3 sum[skyIrradianceCount];
    for (uint index = 0; index < skyIrradianceCount; ++index)
        sum[index] = float3(0.0, 0.0, 0.0);

    for (uint step = 0; step < directionsPerThread; ++step)
    {
        // A spiral from the top of the sphere to the bottom, turned on by the golden angle at every
        // point. The threads take every 64th point each, so each of them sees all of the sphere.
        uint point = step * threadCount + thread.x;
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

    for (uint index = 0; index < skyIrradianceCount; ++index)
        found[thread.x * skyIrradianceCount + index] = sum[index];

    GroupMemoryBarrierWithGroupSync();
    if (thread.x != 0)
        return;

    // The light from each direction, as nine numbers, becomes the light a surface gathers over the
    // half of the sphere it faces by scaling the numbers: by pi for the first, two thirds of pi for
    // the next three, a quarter of pi for the last five.
    static const float gathered[skyIrradianceCount] = { pi, 2.0 * pi / 3.0, 2.0 * pi / 3.0, 2.0 * pi / 3.0, pi / 4.0, pi / 4.0, pi / 4.0, pi / 4.0, pi / 4.0 };
    float perDirection = 4.0 * pi / float(directionCount);
    for (uint index = 0; index < skyIrradianceCount; ++index)
    {
        float3 total = float3(0.0, 0.0, 0.0);
        for (uint other = 0; other < threadCount; ++other)
            total += found[other * skyIrradianceCount + index];
        scene.skyIrradiance[index] = float4(total * (perDirection * gathered[index]), 0.0);
    }
}
