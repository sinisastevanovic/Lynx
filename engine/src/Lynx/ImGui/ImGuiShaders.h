#pragma once

namespace Lynx
{
    static const char* g_ImGui_VertexShader = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant {
    vec2 uScale;
    vec2 uTranslate;
} pc;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out vec2 UV;
layout(location = 1) out vec4 Color;

void main()
{
    Color = aColor;
    UV = aUV;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}
)";

    static const char* g_ImGui_PixelShader = R"(
#version 450 core
layout(location = 0) out vec4 fColor;

layout(set=0, binding=0) uniform texture2D sTexture;
layout(set=0, binding=1) uniform sampler sSampler;

layout(location = 0) in vec2 UV;
layout(location = 1) in vec4 Color;

void main()
{
    fColor = Color * texture(sampler2D(sTexture, sSampler), UV);
}
)";
    
}