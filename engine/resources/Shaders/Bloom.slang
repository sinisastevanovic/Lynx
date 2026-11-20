// The glow around bright parts of the picture. One shader does all three kinds of step, chosen by "mode":
// keep only what is brighter than the threshold, shrink a picture to half its size, and enlarge one to twice its size.
// The pass runs it once per step, each time from one size of the bloom picture into the next.

import Textures;

static const int modePrefilter = 0;
static const int modeDownsample = 1;
static const int modeUpsample = 2;

struct BloomConstants
{
    float threshold;
    float knee;
    float radius;
    int mode;

    // The size of one pixel of the source, in texture coordinates.
    float2 texelSize;

    // Indices into the texture and sampler tables.
    uint sourceTexture;
    uint linearSampler;
};
[vk::push_constant] ConstantBuffer<BloomConstants> constants;

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

float3 sampleSource(float2 uv)
{
    return textures[constants.sourceTexture].Sample(samplers[constants.linearSampler], uv).rgb;
}

// 13 samples in a pattern that keeps a single bright pixel from flickering as it moves (Jimenez, 2014).
float3 downsample(float2 uv)
{
    float x = constants.texelSize.x;
    float y = constants.texelSize.y;

    float3 a = sampleSource(uv + float2(-2 * x, 2 * y));
    float3 b = sampleSource(uv + float2(0, 2 * y));
    float3 c = sampleSource(uv + float2(2 * x, 2 * y));
    float3 d = sampleSource(uv + float2(-2 * x, 0));
    float3 e = sampleSource(uv + float2(0, 0));
    float3 f = sampleSource(uv + float2(2 * x, 0));
    float3 g = sampleSource(uv + float2(-2 * x, -2 * y));
    float3 h = sampleSource(uv + float2(0, -2 * y));
    float3 i = sampleSource(uv + float2(2 * x, -2 * y));
    float3 j = sampleSource(uv + float2(-x, y));
    float3 k = sampleSource(uv + float2(x, y));
    float3 l = sampleSource(uv + float2(-x, -y));
    float3 m = sampleSource(uv + float2(x, -y));

    float3 result = e * 0.125;
    result += (a + c + g + i) * 0.03125;
    result += (b + d + f + h) * 0.0625;
    result += (j + k + l + m) * 0.125;
    return result;
}

// 9 samples weighted like a tent. The radius spreads them out.
float3 upsample(float2 uv)
{
    float x = constants.texelSize.x * constants.radius;
    float y = constants.texelSize.y * constants.radius;

    float3 a = sampleSource(uv + float2(-x, y));
    float3 b = sampleSource(uv + float2(0, y));
    float3 c = sampleSource(uv + float2(x, y));
    float3 d = sampleSource(uv + float2(-x, 0));
    float3 e = sampleSource(uv + float2(0, 0));
    float3 f = sampleSource(uv + float2(x, 0));
    float3 g = sampleSource(uv + float2(-x, -y));
    float3 h = sampleSource(uv + float2(0, -y));
    float3 i = sampleSource(uv + float2(x, -y));

    return (e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i)) * (1.0 / 16.0);
}

// Keeps what is brighter than the threshold. The knee makes the cut soft, so that
// a surface near the threshold does not switch its glow on and off.
float3 prefilter(float3 color)
{
    float brightness = max(color.r, max(color.g, color.b));

    float soft = brightness - constants.threshold + constants.knee;
    soft = clamp(soft, 0.0, 2.0 * constants.knee);
    soft = soft * soft / (4.0 * constants.knee + 0.00001);

    float contribution = max(soft, brightness - constants.threshold);
    contribution /= max(brightness, 0.00001);

    return color * contribution;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    if (constants.mode == modePrefilter)
        return float4(prefilter(sampleSource(input.uv)), 1.0);

    if (constants.mode == modeDownsample)
        return float4(downsample(input.uv), 1.0);

    // The pipeline adds the result to what the target already holds.
    return float4(upsample(input.uv), 1.0);
}
