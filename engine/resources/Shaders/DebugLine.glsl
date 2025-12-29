#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(location = 0) out vec4 v_Color;

layout(set = 0, binding = 0) uniform SceneData
{
    mat4 ViewProjection;
} u_Scene;

void main()
{
    v_Color = a_Color;
    gl_Position = u_Scene.ViewProjection * vec4(a_Position, 1.0);
}

#type pixel
#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = v_Color;
}