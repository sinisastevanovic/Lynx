// Turns the finished HDR picture into what the window shows: anti-aliasing, bloom, tone mapping and gamma.

import Textures;

struct CompositeConstants
{
    float bloomIntensity;
    int fxaaEnabled;

    // Indices into the texture and sampler tables.
    uint sceneTexture;
    uint bloomTexture;
    uint linearSampler;
};
[vk::push_constant] ConstantBuffer<CompositeConstants> constants;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// One triangle that covers the screen, made from the vertex index alone.
[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

float3 sampleScene(float2 uv)
{
    return textures[constants.sceneTexture].Sample(samplers[constants.linearSampler], uv).rgb;
}

float3 acesToneMapping(float3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

static const float fxaaSpanMax = 8.0;
static const float fxaaReduceMul = 1.0 / 8.0;
static const float fxaaReduceMin = 1.0 / 128.0;

// The standard fast FXAA: blurs along the direction of the strongest edge at each pixel.
float3 fxaa(float2 uv, float2 texelSize)
{
    float3 rgbNW = sampleScene(uv + float2(-1.0, -1.0) * texelSize);
    float3 rgbNE = sampleScene(uv + float2(1.0, -1.0) * texelSize);
    float3 rgbSW = sampleScene(uv + float2(-1.0, 1.0) * texelSize);
    float3 rgbSE = sampleScene(uv + float2(1.0, 1.0) * texelSize);
    float3 rgbM = sampleScene(uv);

    const float3 lumaWeights = float3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, lumaWeights);
    float lumaNE = dot(rgbNE, lumaWeights);
    float lumaSW = dot(rgbSW, lumaWeights);
    float lumaSE = dot(rgbSE, lumaWeights);
    float lumaM = dot(rgbM, lumaWeights);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * fxaaReduceMul), fxaaReduceMin);
    float inverseSmallest = 1.0 / (min(abs(direction.x), abs(direction.y)) + directionReduce);

    direction = min(float2(fxaaSpanMax, fxaaSpanMax), max(float2(-fxaaSpanMax, -fxaaSpanMax), direction * inverseSmallest)) * texelSize;

    float3 rgbA = (1.0 / 2.0) * (
        sampleScene(uv + direction * (1.0 / 3.0 - 0.5)) +
        sampleScene(uv + direction * (2.0 / 3.0 - 0.5)));

    float3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (
        sampleScene(uv + direction * (0.0 / 3.0 - 0.5)) +
        sampleScene(uv + direction * (3.0 / 3.0 - 0.5)));

    float lumaB = dot(rgbB, lumaWeights);
    if (lumaB < lumaMin || lumaB > lumaMax)
        return rgbA;
    return rgbB;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    // The scene is drawn with the viewport turned upside down, see CompositePass.cpp. The bloom is not.
    float2 sceneUV = float2(input.uv.x, 1.0 - input.uv.y);

    uint width, height;
    textures[constants.sceneTexture].GetDimensions(width, height);
    float2 texelSize = 1.0 / float2(width, height);

    float3 sceneColor;
    if (constants.fxaaEnabled == 1)
        sceneColor = fxaa(sceneUV, texelSize);
    else
        sceneColor = sampleScene(sceneUV);

    float3 bloomColor = textures[constants.bloomTexture].Sample(samplers[constants.linearSampler], input.uv).rgb;
    sceneColor += bloomColor * constants.bloomIntensity;

    float3 mapped = acesToneMapping(sceneColor);
    mapped = pow(mapped, float3(1.0 / 2.2));
    return float4(mapped, 1.0);
}
