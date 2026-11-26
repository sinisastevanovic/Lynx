// Draws only the depth of the scene's meshes, ahead of the forward pass. With the depth known,
// the forward pass shades every pixel exactly once.

import Mesh;
import Material;

// The same for every draw of the pass.
struct DepthConstants
{
    // The instances of the scene and where they are, the places of those this frame draws, and the
    // material table, which is read for cut-outs only.
    MeshInstance* instances;
    MeshTransform* transforms;
    uint2* visibleInstances;
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
VertexOutput vertexMain(VertexInput input, uint instanceIndex : SV_VulkanInstanceID, uint vertexIndex : SV_VulkanVertexID)
{
    uint2 entry = drawnEntry(constants.visibleInstances, instanceIndex);
    MeshInstance instance = meshInstance(constants.instances, entry);
    float3 position = leafPosition(instance, input.position, vertexIndex, drawnLevel(entry));

    VertexOutput output;
    output.position = meshPosition(meshModel(constants.transforms, entry), position).clip;
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
