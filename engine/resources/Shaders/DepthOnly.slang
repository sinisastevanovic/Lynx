// Draws only the depth of the scene's meshes, ahead of the forward pass. With the depth known,
// the forward pass shades every pixel exactly once.

import Mesh;
import Textures;

struct DepthConstants
{
    // Pixels whose alpha is below this are left out. Negative for a material without cut-outs.
    float alphaCutoff;

    // Indices into the texture and sampler tables, used only with cut-outs.
    uint albedoTexture;
    uint albedoSampler;
    uint padding;

    // The first instance of this draw, in the buffer with the instances of the whole frame.
    MeshInstance* instances;
};
[vk::push_constant] ConstantBuffer<DepthConstants> constants;

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
    VertexOutput output;
    output.position = meshPosition(constants.instances[instanceIndex], input.position).clip;
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
