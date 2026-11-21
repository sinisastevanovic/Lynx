// Draws only the depth of the scene's meshes, ahead of the forward pass. With the depth known,
// the forward pass shades every pixel exactly once.

import Mesh;
import Material;

// The same for every draw of the pass.
struct DepthConstants
{
    // The instances of the whole frame, and the material table, which is read for cut-outs only.
    MeshInstance* instances;
    StandardMaterial* materials;
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
    nointerpolation uint material : MATERIAL;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceIndex : SV_VulkanInstanceID)
{
    MeshInstance instance = constants.instances[instanceIndex];

    VertexOutput output;
    output.position = meshPosition(instance, input.position).clip;
    output.uv = input.uv;
    output.material = instance.material;
    return output;
}

[shader("fragment")]
void fragmentMain(VertexOutput input)
{
    if (isCutOut(constants.materials, input.material, input.uv))
        discard;
}
