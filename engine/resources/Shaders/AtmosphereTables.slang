// Fills the three tables the sky is read from, see Atmosphere.slang. One shader does all three, chosen
// by "table": the pass draws a triangle over each table in turn, and a pixel works out its own entry.

import Atmosphere;
import Scene;

static const uint tableTransmittance = 0;
static const uint tableMultiScattering = 1;
static const uint tableSkyView = 2;

static const float pi = 3.14159265359;

struct TableConstants
{
    uint table;
};
[vk::push_constant] ConstantBuffer<TableConstants> constants;

// One triangle that covers the target, made from the vertex index alone.
[shader("vertex")]
float4 vertexMain(uint vertexIndex : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    return float4(uv * 2.0 - 1.0, 0.0, 1.0);
}

float3 transmittanceEntry(float2 uv)
{
    float radius;
    float upwards;
    transmittanceParameters(uv, radius, upwards);
    return transmittanceToTopBySteps(radius, upwards);
}

// Light after more than one bounce, at a height and for a height of the sun. Rays in 64 directions
// gather what the air sends after one bounce (with the ground's share), and how much of any light is
// scattered again. Every further bounce is the one before times that share, and the sum of all of
// them has a closed form.
float3 multiScatteringEntry(float2 uv)
{
    static const int directionsPerSide = 8;
    static const int stepCount = 20;

    float2 unit = saturate((uv - 0.5 / multiScatteringTableSize) * (multiScatteringTableSize / (multiScatteringTableSize - 1.0)));
    float sunUpwards = unit.x * 2.0 - 1.0;
    float3 sunDirection = float3(sqrt(saturate(1.0 - sunUpwards * sunUpwards)), sunUpwards, 0.0);

    float ground = scene.atmosphere.radii.x;
    float top = scene.atmosphere.radii.y;
    float3 position = float3(0.0, ground + saturate(unit.y + 0.01) * (top - ground - 0.01), 0.0);

    float3 afterOneBounce = float3(0.0, 0.0, 0.0);
    float3 scatteredAgain = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < directionsPerSide; ++i)
    {
        for (int j = 0; j < directionsPerSide; ++j)
        {
            // Evenly over the sphere.
            float around = 2.0 * pi * (float(i) + 0.5) / float(directionsPerSide);
            float fromUp = acos(1.0 - 2.0 * (float(j) + 0.5) / float(directionsPerSide));
            float3 direction = float3(cos(around) * sin(fromUp), cos(fromUp), sin(around) * sin(fromUp));

            Scattered scattered = scatteredAlong(position, direction, sunDirection, stepCount, false, true, false);
            afterOneBounce += scattered.light;
            scatteredAgain += scattered.scatteredAgain;
        }
    }

    float directionCount = float(directionsPerSide * directionsPerSide);
    afterOneBounce /= directionCount;
    scatteredAgain /= directionCount;
    return afterOneBounce / (1.0 - scatteredAgain);
}

float3 skyViewEntry(float2 uv)
{
    static const int stepCount = 32;

    float3 camera = cameraInAtmosphere();
    float upwards;
    float bearing;
    skyViewParameters(uv, camera.y, upwards, bearing);

    // The sun is put at bearing zero, whatever its bearing in the world: only the angle between the
    // two bearings matters.
    float sunUpwards = towardsSun().y;
    float3 sunDirection = float3(sqrt(saturate(1.0 - sunUpwards * sunUpwards)), sunUpwards, 0.0);
    float sideways = sqrt(saturate(1.0 - upwards * upwards));
    float3 direction = float3(sideways * bearing, upwards, sideways * sqrt(saturate(1.0 - bearing * bearing)));

    return scatteredAlong(camera, direction, sunDirection, stepCount, true, false, true).light;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target
{
    float3 entry;
    if (constants.table == tableTransmittance)
        entry = transmittanceEntry(position.xy / transmittanceTableSize);
    else if (constants.table == tableMultiScattering)
        entry = multiScatteringEntry(position.xy / multiScatteringTableSize);
    else
        entry = skyViewEntry(position.xy / skyViewTableSize);
    return float4(entry, 1.0);
}
