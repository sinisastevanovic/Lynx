#type vertex
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

layout(push_constant) uniform PushConsts {
    vec2 ScreenSize;
    float PixelRange;
} u_Push;

void main() {
    outUV = inUV;
    outColor = inColor;

    float x = (inPosition.x / u_Push.ScreenSize.x) * 2.0 - 1.0;
    float y = 1.0 - (inPosition.y / u_Push.ScreenSize.y) * 2.0;

    gl_Position = vec4(x, y, 0.0, 1.0);
}

#type pixel
#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outFragColor;

layout(binding = 0) uniform texture2D u_Texture;
layout(binding = 1) uniform sampler u_Sampler;

layout(push_constant) uniform PushConsts {
    vec2 ScreenSize;
    float PixelRange;
} u_Push;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

vec2 sqr(vec2 x) { return x*x; }

float screenPxRange() {
    vec2 unitRange = vec2(u_Push.PixelRange)/vec2(textureSize(sampler2D(u_Texture, u_Sampler), 0));
    vec2 screenTexSize = inversesqrt(sqr(dFdx(inUV)) + sqr(dFdy(inUV)));
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 msd = texture(sampler2D(u_Texture, u_Sampler), inUV).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    outFragColor = vec4(inColor.rgb, inColor.a * opacity);
}