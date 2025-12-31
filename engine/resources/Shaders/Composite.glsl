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

vec3 ACESToneMapping(vec3 color)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0);
}

void main()
{
    vec2 flippedUV = vec2(v_TexCoord.x, 1.0 - v_TexCoord.y);
    vec3 hdrColor = texture(sampler2D(u_SceneTexture, u_Sampler), flippedUV).rgb;
    vec3 mapped = ACESToneMapping(hdrColor);

    // Gamma Correction
    mapped = pow(mapped, vec3(1.0 / 2.2));
    outColor = vec4(mapped, 1.0);
}