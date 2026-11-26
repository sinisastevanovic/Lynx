// Draws one texture into a picture, for looking at it: one of its sizes, some of its channels, and
// a part of it or all. See TexturePreviewPass. What it draws goes to the screen as it is.

import Textures;

struct PreviewConstants
{
    // The part of the texture the picture shows, in texture coordinates: where it starts and how
    // far it reaches. A part that lies outside the texture is left dark.
    float2 uvStart;
    float2 uvSpan;

    // Where the texture table has the texture and the sampler table the sampler to read it with,
    // and which of the texture's sizes is shown, 0 for the largest.
    uint texture;
    uint textureSampler;
    float size;

    // The channels that are shown, one bit each: red 1, green 2, blue 4, alpha 8.
    uint channels;
};
[vk::push_constant] ConstantBuffer<PreviewConstants> constants;

static const uint showsRed = 1;
static const uint showsGreen = 2;
static const uint showsBlue = 4;
static const uint showsAlpha = 8;

// What lies around the texture, and the two greys of the squares that show through where it is see-through.
static const float3 around = float3(0.09, 0.09, 0.1);
static const float3 lightSquare = float3(0.45, 0.45, 0.45);
static const float3 darkSquare = float3(0.3, 0.3, 0.3);
static const float squareSize = 12.0;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// One triangle that covers the picture, made from the vertex index alone.
[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 uv = constants.uvStart + input.uv * constants.uvSpan;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0)
        return float4(around, 1.0);

    float4 texel = textures[constants.texture].SampleLevel(samplers[constants.textureSampler], uv, constants.size);

    // A single channel is shown as a grey, which says more than a shade of red does.
    uint colours = constants.channels & (showsRed | showsGreen | showsBlue);
    bool hasAlpha = (constants.channels & showsAlpha) != 0;
    if (colours == 0)
        return float4(hasAlpha ? texel.aaa : float3(0.0, 0.0, 0.0), 1.0);

    float3 shown = float3(
        (colours & showsRed) != 0 ? texel.r : 0.0,
        (colours & showsGreen) != 0 ? texel.g : 0.0,
        (colours & showsBlue) != 0 ? texel.b : 0.0);
    if (colours == showsRed)
        shown = texel.rrr;
    else if (colours == showsGreen)
        shown = texel.ggg;
    else if (colours == showsBlue)
        shown = texel.bbb;

    // With the alpha, the texture lies on squares that show through where it is see-through.
    if (hasAlpha)
    {
        float2 square = floor(input.position.xy / squareSize);
        float3 behind = fmod(square.x + square.y, 2.0) < 1.0 ? lightSquare : darkSquare;
        shown = lerp(behind, shown, texel.a);
    }
    return float4(shown, 1.0);
}
