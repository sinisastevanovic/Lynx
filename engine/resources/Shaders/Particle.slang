// Particles: one square per particle that always faces the camera, with a texture that can hold
// the frames of an animation in a grid.

import Scene;
import Textures;

// Has to match ParticleInstanceData in RenderPass.h.
struct ParticleData
{
    float3 position;
    float rotation;
    float4 color;
    float size;

    // 1 when the particle is born, 0 when it dies.
    float life;
    float2 padding;
};

struct ParticleConstants
{
    float4 albedoColor;

    // How many frames of animation the texture holds: columns and rows.
    float2 tiling;
    float emissiveStrength;

    // Indices into the texture and sampler tables.
    uint albedoTexture;

    // The first particle of this draw, in the buffer with the particles of the whole frame.
    ParticleData* particles;
    uint albedoSampler;
    uint padding;
};
[vk::push_constant] ConstantBuffer<ParticleConstants> constants;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

// A square of two triangles, made from the vertex index alone.
static const float2 cornerPositions[4] = { float2(-0.5, -0.5), float2(0.5, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
static const float2 cornerUVs[4] = { float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, 0.0), float2(0.0, 0.0) };
static const uint cornerOfVertex[6] = { 0, 1, 2, 2, 3, 0 };

[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID, uint instanceIndex : SV_InstanceID)
{
    ParticleData particle = constants.particles[instanceIndex];
    uint corner = cornerOfVertex[vertexIndex];

    // The square is sized and turned around its centre first.
    float2 scaled = cornerPositions[corner] * particle.size;
    float sine = sin(particle.rotation);
    float cosine = cos(particle.rotation);
    float2 rotated = float2(scaled.x * cosine - scaled.y * sine, scaled.x * sine + scaled.y * cosine);

    // Then it is laid out along two directions at right angles to the line from the particle to the camera.
    float3 toCamera = normalize(scene.cameraPosition.xyz - particle.position);
    float3 right = normalize(cross(toCamera, float3(0.0, 1.0, 0.0)));
    float3 up = cross(right, toCamera);
    float3 worldPosition = particle.position + right * rotated.x + up * rotated.y;

    VertexOutput output;
    output.position = mul(scene.viewProjection, float4(worldPosition, 1.0));
    output.color = particle.color;
    output.uv = cornerUVs[corner];

    // An animated texture shows the frame that fits the particle's age.
    float frameCount = constants.tiling.x * constants.tiling.y;
    if (frameCount > 1.0)
    {
        float frame = clamp(floor((1.0 - particle.life) * frameCount), 0.0, frameCount - 1.0);
        float column = frame - constants.tiling.x * floor(frame / constants.tiling.x);
        float row = floor(frame / constants.tiling.x);

        output.uv.x = (1.0 - output.uv.x + column) / constants.tiling.x;
        output.uv.y = (output.uv.y + row) / constants.tiling.y;
    }

    return output;
}

// The targets in the order the render graph gives them to a pass: the colour, then how much of the
// pixel the particle covers. A particle writes no motion, and the second says so to whoever puts
// frames together.
struct FragmentOutput
{
    float4 color : SV_Target0;
    float reactive : SV_Target1;
};

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    float4 color = textures[constants.albedoTexture].Sample(samplers[constants.albedoSampler], input.uv);
    color *= input.color * constants.albedoColor;
    color.rgb *= constants.emissiveStrength;

    FragmentOutput output;
    output.color = color;
    output.reactive = saturate(color.a);
    return output;
}
