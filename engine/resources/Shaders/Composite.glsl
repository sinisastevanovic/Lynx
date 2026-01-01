#type vertex
#version 450
layout (location = 0) out vec2 v_TexCoord;

void main()
{
    // Fullscreen Triangle trick
    v_TexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(v_TexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
}

#type pixel
#version 450
layout (location = 0) in vec2 v_TexCoord;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform texture2D u_SceneTexture;
layout (set = 0, binding = 1) uniform sampler u_Sampler;
layout (set = 0, binding = 2) uniform texture2D u_BloomTexture;

layout(push_constant) uniform PushConsts {
    float u_BloomIntensity;
    int u_FXAAEnabled;
} push;

vec3 ACESToneMapping(vec3 color)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0);
}

// FXAA Implementation
// Based on standard fast implementation
#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL   (1.0/8.0)
#define FXAA_REDUCE_MIN   (1.0/128.0)

vec3 FXAA(vec2 uv, vec2 inverseVP) {
    vec3 rgbNW = texture(sampler2D(u_SceneTexture, u_Sampler), uv + (vec2(-1.0, -1.0) * inverseVP)).xyz;
    vec3 rgbNE = texture(sampler2D(u_SceneTexture, u_Sampler), uv + (vec2(1.0, -1.0) * inverseVP)).xyz;
    vec3 rgbSW = texture(sampler2D(u_SceneTexture, u_Sampler), uv + (vec2(-1.0, 1.0) * inverseVP)).xyz;
    vec3 rgbSE = texture(sampler2D(u_SceneTexture, u_Sampler), uv + (vec2(1.0, 1.0) * inverseVP)).xyz;
    vec3 rgbM  = texture(sampler2D(u_SceneTexture, u_Sampler), uv).xyz;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) * inverseVP;

    vec3 rgbA = (1.0/2.0) * (
        texture(sampler2D(u_SceneTexture, u_Sampler), uv + dir * (1.0/3.0 - 0.5)).xyz +
        texture(sampler2D(u_SceneTexture, u_Sampler), uv + dir * (2.0/3.0 - 0.5)).xyz);

    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        texture(sampler2D(u_SceneTexture, u_Sampler), uv + dir * (0.0/3.0 - 0.5)).xyz +
        texture(sampler2D(u_SceneTexture, u_Sampler), uv + dir * (3.0/3.0 - 0.5)).xyz);

    float lumaB = dot(rgbB, luma);

    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        return rgbA;
    } else {
        return rgbB;
    }
}

void main()
{
    vec2 flippedUV = vec2(v_TexCoord.x, 1.0 - v_TexCoord.y);
    vec2 texelSize = 1.0 / textureSize(sampler2D(u_SceneTexture, u_Sampler), 0);
    vec3 sceneColor;
    if (push.u_FXAAEnabled == 1) {
        sceneColor = FXAA(flippedUV, texelSize);
    } else {
        sceneColor = texture(sampler2D(u_SceneTexture, u_Sampler), flippedUV).rgb;
    }

    vec3 bloomColor = texture(sampler2D(u_BloomTexture, u_Sampler), v_TexCoord).rgb;

    sceneColor += bloomColor * push.u_BloomIntensity;

    vec3 mapped = ACESToneMapping(sceneColor);

    // Gamma Correction
    mapped = pow(mapped, vec3(1.0 / 2.2));
    outColor = vec4(mapped, 1.0);
}