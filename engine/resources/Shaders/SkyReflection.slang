// Fills the picture shiny surfaces mirror the sky from: the sky and the planet's ground in every
// direction, in several sizes. The largest is the sky as it is, for a perfect mirror. Each smaller one
// is the sky blurred the way a rougher surface blurs what it mirrors, so a pixel only has to read the
// size that goes with its roughness (the first half of the split sum of "Real Shading in Unreal Engine 4",
// Karis, 2013). See skyReflection in Atmosphere.slang for how directions map to the picture.
//
// The pass runs this once per size, every frame, because the sky changes with the sun.

import Atmosphere;

static const float pi = 3.14159265359;

struct ReflectionConstants
{
    // The roughness this size is blurred for, from 0 to 1.
    float roughness;

    // The width and height of this size in pixels.
    float size;
};
[vk::push_constant] ConstantBuffer<ReflectionConstants> constants;

// How many directions a pixel of a blurred size looks in. The same ones for every pixel, so too few
// would show as a pattern, not as noise. The sky is smooth but for the glare around the sun.
static const uint sampleCount = 64;

// One triangle that covers the target, made from the vertex index alone.
[shader("vertex")]
float4 vertexMain(uint vertexIndex : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    return float4(uv * 2.0 - 1.0, 0.0, 1.0);
}

float3 lightFrom(float3 direction)
{
    return skyLight(direction) + groundLight(direction);
}

// The point with this number of a set that covers a square evenly (Hammersley).
float2 pointInSquare(uint index)
{
    uint bits = index;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return float2((float(index) + 0.5) / float(sampleCount), float(bits) * 2.3283064365386963e-10);
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target
{
    float3 mirrored = skyReflectionDirection(position.xy / constants.size);
    if (constants.roughness <= 0.0)
        return float4(lightFrom(mirrored), 1.0);

    // Two directions at right angles to the mirrored one.
    float3 up = abs(mirrored.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 side = normalize(cross(up, mirrored));
    float3 forward = cross(mirrored, side);

    // A rough surface is many tiny mirrors tilted every which way (the GGX distribution, the one the
    // sun's highlight uses). Each sample picks one such mirror, more often a likely one, and looks
    // where it mirrors to. The surface is taken to be looked at head on, which is the usual shortcut.
    float spread = constants.roughness * constants.roughness;
    float3 light = float3(0.0, 0.0, 0.0);
    float weight = 0.0;
    for (uint index = 0; index < sampleCount; ++index)
    {
        float2 point = pointInSquare(index);
        float around = 2.0 * pi * point.x;
        float cosTilt = sqrt((1.0 - point.y) / (1.0 + (spread * spread - 1.0) * point.y));
        float sinTilt = sqrt(saturate(1.0 - cosTilt * cosTilt));
        float3 mirror = side * (cos(around) * sinTilt) + forward * (sin(around) * sinTilt) + mirrored * cosTilt;

        float3 towards = 2.0 * dot(mirrored, mirror) * mirror - mirrored;
        float facing = dot(mirrored, towards);
        if (facing > 0.0)
        {
            light += lightFrom(towards) * facing;
            weight += facing;
        }
    }
    return float4(light / max(weight, 1e-4), 1.0);
}
