#type vertex
#version 450

layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_Color;

struct SceneData
{
    mat4 ViewProjection;
    mat4 LightViewProj;
    vec4 CameraPosition;
    vec4 LightDirection;
    vec4 LightColor;
};

layout(binding = 0) uniform SceneConstantBuffer
{
    SceneData u_Scene;
};

struct ParticleData
{
    vec3 Position;
    float Rotation;
    vec4 Color;
    float Size;
    float Life; // TODO: Use this for sheet animation!
    vec2 Padding;
};

layout(binding = 1) readonly buffer ParticleBuffer
{
    ParticleData particles[];
};

layout(push_constant) uniform PushConstants
{
    vec4 u_AlbdeoColor;
    float u_EmissiveStrength;
    float u_Padding[3];
} push;

void main()
{
    ParticleData data = particles[gl_InstanceIndex];

    // Billboarding Math
    // 1. Get Camera Right and Up vectors from View Matrix
    // The View Matrix is Inverse Camera Transform.
    // Row 0 is Right, Row 1 is Up, Row 2 is Forward (approx).
    // Note: Depends on GLM layout (Column Major).
    // View[0][0], View[1][0], View[2][0] is Right Vector
    // View[0][1], View[1][1], View[2][1] is Up Vector

    // Simple way:
    vec3 cameraRight = vec3(u_Scene.ViewProjection[0][0], u_Scene.ViewProjection[1][0], u_Scene.ViewProjection[2][0]);
    vec3 cameraUp    = vec3(u_Scene.ViewProjection[0][1], u_Scene.ViewProjection[1][1], u_Scene.ViewProjection[2][1]);

    // Cleaner way if you pass InverseView, but this works for standard LookAt matrices.

    // 2. Scale
    vec3 vertexPos = vec3(a_Pos * data.Size, 0.0);

    // 3. Rotation (2D rotation around Z)
    float s = sin(data.Rotation);
    float c = cos(data.Rotation);
    vec2 rotatedPos = vec2(
        vertexPos.x * c - vertexPos.y * s,
        vertexPos.x * s + vertexPos.y * c
    );

    // 4. Calculate World Position
    // We construct the billboard by adding Right*X and Up*Y to the Center
    // This ignores camera rotation essentially, keeping it flat to screen.
    // Actually, "Spherical Billboarding":

    // Extract camera basis from View Matrix (assuming ViewProjection includes View)
    // It's safer to just look at camera position, but for exact billboarding we need the vectors.
    // Let's rely on the quad being aligned to View Space if we multiplied by View first.

    // Alternative:
    // WorldPos = Center + CameraRight * x + CameraUp * y
    // We need Camera vectors in World Space.
    // We can extract them from the Inverse View Matrix if we had it.
    // Or we can construct a LookAt matrix per particle (expensive).

    // Most efficient robust billboard:
    vec3 camPos = u_Scene.CameraPosition.xyz;
    vec3 toCam = normalize(camPos - data.Position);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(toCam, up));
    up = cross(right, toCam); // Re-orthogonalize

    vec3 worldPos = data.Position
        + right * rotatedPos.x
        + up * rotatedPos.y;

    gl_Position = u_Scene.ViewProjection * vec4(worldPos, 1.0);

    v_UV = a_UV;
    v_Color = data.Color;
}

#type pixel
#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 o_Color;

layout(set = 1, binding = 0) uniform texture2D u_AlbedoMap;
layout(set = 1, binding = 1) uniform sampler u_Sampler;

layout(push_constant) uniform PushConstants
{
    vec4 u_AlbdeoColor;
    float u_EmissiveStrength;
    float u_Padding[3];
} push;

void main() {
    vec4 texColor = texture(sampler2D(u_AlbedoMap, u_Sampler), v_UV);
    vec4 finalColor = texColor * v_Color * push.u_AlbdeoColor;
    if (o_Color.a < 0.01)
        discard;
    finalColor.rgb *= push.u_EmissiveStrength;
    o_Color = finalColor;
}