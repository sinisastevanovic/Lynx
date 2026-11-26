// Draws the depth of the scene's meshes as the sun sees them. The forward pass compares against it
// to find out which surfaces the sun reaches.

import Mesh;
import Material;

// The same for every draw of the pass.
struct ShadowConstants
{
    // From the world to the sun's view of it.
    float4x4 lightViewProjection;

    // The instances of the scene and where they are, the places of those this frame draws, and the
    // material table, which is read for cut-outs only.
    MeshInstance* instances;
    MeshTransform* transforms;
    uint2* visibleInstances;
    StandardMaterial* materials;
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
    nointerpolation uint material : MATERIAL;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceIndex : SV_VulkanInstanceID, uint vertexIndex : SV_VulkanVertexID)
{
    uint2 entry = drawnEntry(constants.visibleInstances, instanceIndex);
    MeshInstance instance = meshInstance(constants.instances, entry);
    float3 position = leafPosition(instance, input.position, vertexIndex, drawnLevel(entry));

    VertexOutput output;
    float4x4 model = meshModel(constants.transforms, entry);
    output.position = mul(mul(constants.lightViewProjection, model), float4(position, 1.0));
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
