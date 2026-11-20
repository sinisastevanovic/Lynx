// The in-game UI: textured, tinted rectangles, positioned in pixels with the origin at the top left.

import Textures;

struct UIConstants
{
    float2 screenSize;

    // Only used for text.
    float pixelRange;

    // Indices into the texture and sampler tables.
    uint textureIndex;
    uint samplerIndex;
    uint padding;
};
[vk::push_constant] ConstantBuffer<UIConstants> constants;

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;

    // Left, top, right and bottom of the area the element may draw in, in pixels.
    float4 clipRect : CLIPRECT;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    float4 clipRect : CLIPRECT;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.uv = input.uv;
    output.color = input.color;
    output.clipRect = input.clipRect;

    float x = (input.position.x / constants.screenSize.x) * 2.0 - 1.0;
    float y = 1.0 - (input.position.y / constants.screenSize.y) * 2.0;
    output.position = float4(x, y, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 pixel = input.position.xy;
    if (pixel.x < input.clipRect.x || pixel.y < input.clipRect.y || pixel.x > input.clipRect.z || pixel.y > input.clipRect.w)
        discard;

    return input.color * textures[constants.textureIndex].Sample(samplers[constants.samplerIndex], input.uv);
}
