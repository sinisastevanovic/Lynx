#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Tangent;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_Color;

layout(location = 0) out vec2 v_TexCoord;

layout(set = 0, binding = 0) uniform UBO {
    mat4 u_ViewProjection;
} ubo;

layout(push_constant) uniform PushConsts {
    mat4 u_Model;
    vec4 u_Color;
    float u_AlphaCutoff;
} push;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = ubo.u_ViewProjection * push.u_Model * vec4(a_Position, 1.0);
}

#type pixel
#version 450

layout(location = 0) in vec2 v_TexCoord;

layout(push_constant) uniform PushConsts {
    mat4 u_Model;
    vec4 u_Color;
    float u_AlphaCutoff;
} push;

layout(set = 0, binding = 1) uniform texture2D u_AlbedoMap;
layout(set = 0, binding = 2) uniform sampler u_Sampler;

void main() {
    if (push.u_AlphaCutoff >= 0.0) {
        float alpha = texture(sampler2D(u_AlbedoMap, u_Sampler), v_TexCoord).a;
        if (alpha < push.u_AlphaCutoff)
            discard;
    }
}