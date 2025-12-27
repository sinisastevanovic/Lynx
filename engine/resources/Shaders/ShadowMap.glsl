#type vertex
#version 450
layout(location = 0) in vec3 a_Position; // TODO: Add alpha masking

layout(push_constant) uniform PushConsts {
    mat4 u_Model;
    // TODO: add alpha cutoff later
} push;

layout(set = 0, binding = 0) uniform UBO {
    mat4 u_LightViewProj;
} ubo;

void main() {
    gl_Position = ubo.u_LightViewProj * push.u_Model * vec4(a_Position, 1.0);
}

#type pixel
#version 450
void main() {
    
}