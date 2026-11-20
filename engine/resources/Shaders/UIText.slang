// Text in the in-game UI. The font texture holds, for every glyph, the distance to its outline in three
// channels (a multi-channel signed distance field), which stays sharp at any size and gives outlines for free.

import Textures;

struct UIConstants
{
    float2 screenSize;

    // How many pixels of the font texture the distances are spread over.
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
    float4 outlineColor : OUTLINECOLOR;

    // In units of the distance field: 0 for no outline, 0.5 for the widest one.
    float outlineWidth : OUTLINEWIDTH;

    // Left, top, right and bottom of the area the element may draw in, in pixels.
    float4 clipRect : CLIPRECT;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    float4 outlineColor : OUTLINECOLOR;
    float outlineWidth : OUTLINEWIDTH;
    float4 clipRect : CLIPRECT;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.uv = input.uv;
    output.color = input.color;
    output.outlineColor = input.outlineColor;
    output.outlineWidth = input.outlineWidth;
    output.clipRect = input.clipRect;

    float x = (input.position.x / constants.screenSize.x) * 2.0 - 1.0;
    float y = 1.0 - (input.position.y / constants.screenSize.y) * 2.0;
    output.position = float4(x, y, 0.0, 1.0);
    return output;
}

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

// How many screen pixels the distance range covers at this pixel. At least 1, so that tiny text does not vanish.
float screenPixelRange(float2 uv)
{
    uint width, height;
    textures[constants.textureIndex].GetDimensions(width, height);

    float2 unitRange = float2(constants.pixelRange, constants.pixelRange) / float2(width, height);
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    float2 screenTextureSize = rsqrt(dx * dx + dy * dy);
    return max(0.5 * dot(unitRange, screenTextureSize), 1.0);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 pixel = input.position.xy;
    if (pixel.x < input.clipRect.x || pixel.y < input.clipRect.y || pixel.x > input.clipRect.z || pixel.y > input.clipRect.w)
        discard;

    float3 distances = textures[constants.textureIndex].Sample(samplers[constants.samplerIndex], input.uv).rgb;
    float signedDistance = median(distances.r, distances.g, distances.b);

    float range = screenPixelRange(input.uv);
    float opacity = clamp(range * (signedDistance - 0.5) + 0.5, 0.0, 1.0);

    float4 color = input.color;
    color.a *= opacity;

    if (input.outlineWidth > 0.0)
    {
        // The outline reaches outwards from the glyph's edge, which lies at 0.5.
        float outlineOpacity = clamp(range * (signedDistance - (0.5 - input.outlineWidth)) + 0.5, 0.0, 1.0);

        float4 outline = input.outlineColor;
        outline.a *= outlineOpacity;

        // Inside the glyph its own colour wins, outside of it the outline's.
        color = lerp(outline, color, opacity);
    }

    return color;
}
