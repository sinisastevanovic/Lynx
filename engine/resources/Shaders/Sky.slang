// Draws the sky, the sun's disc and the planet's bare ground wherever the scene has drawn nothing: one triangle over the screen
// at the farthest depth, which only passes the depth test where the depth buffer is still empty.
// The sky's light comes from the sky view table, see Atmosphere.slang.

import Atmosphere;
import Scene;

struct SkyConstants
{
    float4x4 inverseViewProjection;
};
[vk::push_constant] ConstantBuffer<SkyConstants> constants;

struct VertexOutput
{
    float4 position : SV_Position;

    // Two points on the pixel's view ray, in world space: on the near plane, and further out.
    float3 nearPoint : NEAR_POINT;
    float3 farPoint : FAR_POINT;
};

float3 toWorld(float2 corner, float depth)
{
    float4 world = mul(constants.inverseViewProjection, float4(corner, depth, 1.0));
    return world.xyz / world.w;
}

// One triangle that covers the screen, made from the vertex index alone.
[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    float2 corner = float2((vertexIndex << 1) & 2, vertexIndex & 2) * 2.0 - 1.0;

    VertexOutput output;
    // Depth 0 is the farthest there is, and what the depth buffer holds where nothing was drawn.
    output.position = float4(corner, 0.0, 1.0);
    // Depth runs from 1 at the near plane towards 0 at infinity, which is no point to compute with.
    output.nearPoint = toWorld(corner, 1.0);
    output.farPoint = toWorld(corner, 0.5);
    return output;
}

// The targets in the order the render graph gives them to a pass: the colour, then the motion.
struct FragmentOutput
{
    float4 colour : SV_Target0;
    float2 motion : SV_Target1;
};

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    float3 direction = normalize(input.farPoint - input.nearPoint);

    // The sky is infinitely far away: it moves on screen as the camera turns, and not as it travels.
    float4 clip = mul(scene.viewProjection, float4(direction, 0.0));
    float4 previousClip = mul(scene.previousViewProjection, float4(direction, 0.0));

    FragmentOutput output;
    output.colour = float4(skyLight(direction) + groundLight(direction) + sunDisc(direction, scene.shadowSoftening), 1.0);
    output.motion = screenMotion(clip.xyw, previousClip.xyw);
    return output;
}
