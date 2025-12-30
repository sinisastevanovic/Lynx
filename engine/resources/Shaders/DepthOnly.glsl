#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord; // Needed for Masked materials

layout(location = 0) out vec2 v_TexCoord;

layout(set = 0, binding = 0) uniform UBO {
    mat4 u_ViewProjection;
    mat4 u_LightViewProjection;
    vec4 u_CameraPosition;
    vec4 u_LightDirection;
    vec4 u_LightColor;
} ubo;

struct InstanceData
{
    mat4 Model;
    vec4 Color;
    int EntityID;
    float Padding[3];
};

layout(std430, set = 0, binding = 10) readonly buffer InstanceBuffer {
    InstanceData instances[];
} u_Instances;

void main() {
    InstanceData data = u_Instances.instances[gl_InstanceIndex];
    v_TexCoord = a_TexCoord;
    gl_Position = ubo.u_ViewProjection * data.Model * vec4(a_Position, 1.0);
}

#type pixel
#version 450

layout(location = 0) in vec2 v_TexCoord;

layout(push_constant) uniform PushConsts {
    float u_AlphaCutoff;
} push;

// Set 1: Material
layout(set = 1, binding = 0) uniform texture2D u_AlbedoMap;
layout(set = 1, binding = 1) uniform sampler u_Sampler;

void main() {
    if (push.u_AlphaCutoff >= 0.0) {
        float alpha = texture(sampler2D(u_AlbedoMap, u_Sampler), v_TexCoord).a;
        if (alpha < push.u_AlphaCutoff)
            discard;
    }
}