// What the shaders that draw meshes share: how an instance is stored, and how a vertex gets to the screen.
module Mesh;

import Scene;

// One drawn copy of a mesh. Has to match GPUInstanceData in RenderPass.h.
public struct MeshInstance
{
    public float4x4 model;

    // Written to the picking buffer in the editor. -1 for none.
    public int entityId;
    int padding0;
    int padding1;
    int padding2;
};

// Where a vertex is in the world and on the screen.
public struct MeshPosition
{
    public float4 world;
    public float4 clip;
};

// The depth pass and the forward pass both go through here, so that they arrive at exactly the same
// position: the forward pass draws only where the depth equals what the depth pass wrote.
public MeshPosition meshPosition(MeshInstance instance, float3 position)
{
    MeshPosition result;
    result.world = mul(instance.model, float4(position, 1.0));
    result.clip = mul(scene.viewProjection, result.world);
    return result;
}
