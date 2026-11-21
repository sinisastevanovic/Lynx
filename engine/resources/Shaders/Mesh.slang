// What the shaders that draw meshes share: how an instance is stored, and how a vertex gets to the screen.
module Mesh;

import Scene;

// One drawn copy of a mesh. Has to match GPUInstanceData in RenderPass.h.
public struct MeshInstance
{
    public float4x4 model;

    // Written to the picking buffer in the editor. -1 for none.
    public int entityId;

    // The place of the instance's material in the material table, see Material.slang.
    public uint material;
    int padding0;
    int padding1;
};

// The semantic to give a vertex shader's instance index: SV_VulkanInstanceID counts from the first
// instance of the draw call, as the instance buffer does. SV_InstanceID would start at zero in every draw.

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

// A direction along the surface, such as a tangent, turned and scaled with the instance. Not normalised.
public float3 meshDirection(MeshInstance instance, float3 direction)
{
    return mul((float3x3)instance.model, direction);
}

// A normal of the mesh in the world. Not normalised.
//
// A normal cannot be transformed like a direction along the surface: on an object that is stretched
// along one axis, it would lean the wrong way. What keeps it at a right angle to the surface is the
// matrix of cross products of the model's axes, which costs three cross products instead of a full inverse.
public float3 meshNormal(MeshInstance instance, float3 normal)
{
    float3 axisX = meshDirection(instance, float3(1.0, 0.0, 0.0));
    float3 axisY = meshDirection(instance, float3(0.0, 1.0, 0.0));
    float3 axisZ = meshDirection(instance, float3(0.0, 0.0, 1.0));

    float3 transformed = cross(axisY, axisZ) * normal.x + cross(axisZ, axisX) * normal.y + cross(axisX, axisY) * normal.z;

    // A mirrored instance turns the cross products around.
    return dot(axisX, cross(axisY, axisZ)) < 0.0 ? -transformed : transformed;
}
