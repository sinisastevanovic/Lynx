#type vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec4 inClipRect;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outClipRect;

layout(push_constant) uniform PushConsts {
    vec2 ScreenSize;
} u_Push;

void main() {
    outUV = inUV;
    outColor = inColor;
    outClipRect = inClipRect;

    float x = (inPosition.x / u_Push.ScreenSize.x) * 2.0 - 1.0;
    float y = 1.0 - (inPosition.y / u_Push.ScreenSize.y) * 2.0;

    gl_Position = vec4(x, y, 0.0, 1.0);
}

#type pixel
#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inClipRect;

layout(location = 0) out vec4 outFragColor;

layout(binding = 0) uniform texture2D u_Texture;
layout(binding = 1) uniform sampler u_Sampler;

void main() {

    if (gl_FragCoord.x < inClipRect.x || gl_FragCoord.y < inClipRect.y ||
        gl_FragCoord.x > inClipRect.z || gl_FragCoord.y > inClipRect.w)
    {
        discard;
    }

    outFragColor = inColor * texture(sampler2D(u_Texture, u_Sampler), inUV);
}