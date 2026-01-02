#type vertex
#version 450

layout(location = 0) out vec2 v_UV;

void main()
{
    // Generates a triangle covering the screen: (0,0), (2,0), (0,2)
    // UVs: (0,0), (2,0), (0,2)
    // Clip Space: (-1,-1), (3,-1), (-1,3)
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    v_UV = vec2(uv.x, 1.0 - uv.y); // Flip Y
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#type pixel
#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform texture2D t_Texture;
layout(binding = 1) uniform sampler s_Sampler;

void main()
{
    o_Color = texture(sampler2D(t_Texture, s_Sampler), v_UV);
}