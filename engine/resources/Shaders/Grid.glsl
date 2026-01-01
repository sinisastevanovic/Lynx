#type vertex
#version 450

layout(location = 0) out vec3 v_NearPoint;
layout(location = 1) out vec3 v_FarPoint;

layout(push_constant) uniform PushConsts
{
    mat4 InvView;
    mat4 InvProj;
};

vec3 gridPlane[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

void main() {
    vec3 p = gridPlane[gl_VertexIndex];

    // Project screen coordinates to world space at the Near plane (z=0) and Far plane (z=1)
    vec4 near = InvView * InvProj * vec4(p.x, p.y, 0.0, 1.0);
    vec4 far = InvView * InvProj * vec4(p.x, p.y, 1.0, 1.0);

    v_NearPoint = near.xyz / near.w;
    v_FarPoint = far.xyz / far.w;

    // Output the clip-space vertex position
    gl_Position = vec4(p, 1.0);
}

#type pixel
#version 450

layout(location = 0) in vec3 v_NearPoint;
layout(location = 1) in vec3 v_FarPoint;

layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 0) uniform UBO {
    mat4 u_ViewProjection;
    mat4 u_LightViewProjection;
    vec4 u_CameraPosition;
    vec4 u_LightDirection;
    vec4 u_LightColor;
} ubo;


// Calculates the correct depth value for the hardware depth buffer
float computeDepth(vec3 pos) {
    vec4 clip_space_pos = ubo.u_ViewProjection * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

void main() {
    // Solve for intersection with the XZ plane (y = 0)
    // Ray equation: P = Near + t * (Far - Near)
    // 0 = Near.y + t * (Far.y - Near.y)
    float t = -v_NearPoint.y / (v_FarPoint.y - v_NearPoint.y);

    // If t is negative, the grid plane is "behind" the ray
    if (t <= 0.0) discard;

    vec3 worldPos = v_NearPoint + t * (v_FarPoint - v_NearPoint);

    // Procedural grid calculation
    vec2 coord = worldPos.xz;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);

    // Base grid color
    vec4 color = vec4(0.1, 0.1, 0.1, 1.0 - min(line, 1.0));

    // Highlight major axes
    float axisWidth = 1.5; // Controls axis line thickness

    // Z Axis (Blue) - Where X is near 0
    if (abs(worldPos.x) < derivative.x * axisWidth)
        color = vec4(0.0, 0.0, 0.8, 1.0 - min(line, 1.0));

    // X Axis (Red) - Where Z is near 0
    if (abs(worldPos.z) < derivative.y * axisWidth)
        color = vec4(0.8, 0.0, 0.0, 1.0 - min(line, 1.0));

    // Calculate distance fade to prevent moire patterns and horizon artifacts
    float dist = distance(ubo.u_CameraPosition.xyz, worldPos);
    float fadeStart = 10.0;
    float fadeEnd = 100.0;
    float alpha = 1.0 - clamp((dist - fadeStart) / (fadeEnd - fadeStart), 0.0, 1.0);

    color.a *= alpha;

    // Final visibility check
    if (color.a <= 0.05) discard;

    // Output the correct depth so objects can occlude the grid
    gl_FragDepth = computeDepth(worldPos);

    o_Color = color;
}