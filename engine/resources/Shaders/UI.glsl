#type vertex
#version 460
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;

struct GPUUIData
{
    vec2 Position;
    vec2 Size;
    vec4 Color;
};

layout(std430, binding = 0) readonly buffer InstanceBuffer
{
    GPUUIData instances[];
};

layout(push_constant) uniform Constants
{
    mat4 u_Projection;
};

void main()
{
    GPUUIData data = instances[gl_InstanceIndex];

    // Scale
    vec2 worldPos = (a_Pos * data.Size) + data.Position;

    gl_Position = u_Projection * vec4(worldPos, 0.0, 1.0);
    v_UV = a_UV;
    v_Color = data.Color;
}

#type pixel
#version 450
layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

layout(set = 1, binding = 0) uniform texture2D u_Texture;
layout(set = 1, binding = 1) uniform sampler u_Sampler;

void main()
{
    o_Color = texture(sampler2D(u_Texture, u_Sampler), v_UV) * v_Color;
}