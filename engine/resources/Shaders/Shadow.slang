// Draws the depth of the scene's meshes as the sun sees them. The forward pass compares against it
// to find out which surfaces the sun reaches.

import Mesh;
import Textures;

struct ShadowConstants
{
    // From the world to the sun's view of it.
    float4x4 lightViewProjection;

    // Pixels whose alpha is below this are left out. Negative for a material without cut-outs.
    float alphaCutoff;

    // Indices into the texture and sampler tables, used only with cut-outs.
    uint albedoTexture;
    uint albedoSampler;
    uint padding;

    // The first instance of this draw, in the buffer with the instances of the whole frame.
    MeshInstance* instances;

    // The matrix makes the size a multiple of 16 bytes.
    uint64_t padding2;
};
[vk::push_constant] ConstantBuffer<ShadowConstants> constants;

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceIndex : SV_InstanceID)
{
    MeshInstance instance = constants.instances[instanceIndex];

    VertexOutput output;
    output.position = mul(mul(constants.lightViewProjection, instance.model), float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
void fragmentMain(VertexOutput input)
{
    if (constants.alphaCutoff >= 0.0)
    {
        float alpha = textures[constants.albedoTexture].Sample(samplers[constants.albedoSampler], input.uv).a;
        if (alpha < constants.alphaCutoff)
            discard;
    }
}
