// What the shaders that draw meshes share: how an instance is stored, and how a vertex gets to the screen.
module Mesh;

import Scene;

// What a vertex of foliage knows about the leaf it is part of. Has to match LeafVertex in StaticMesh.h,
// which stores four numbers of 16 bits: they are read as two of 32 here.
public struct LeafVertex
{
    // The centre of the leaf in x and y: from the centre of the sphere around the submesh, in units of
    // its radius, as numbers from -32767 to 32767.
    public uint pivotXY;

    // The same for z in the lower half. Above it, a byte each, in eighths of a level: the last level of
    // detail the leaf is drawn in, and the level from which on it is made larger.
    public uint pivotZAndLevels;
};

// One submesh of one mesh in the scene. The instances of the whole scene are in one buffer that stays
// on the GPU from frame to frame. Has to match GpuInstance in GpuScene.h.
public struct MeshInstance
{
    public float4x4 model;

    // A sphere around the submesh in the mesh's own space: the centre in xyz, the radius in w.
    public float4 boundingSphere;

    // The submesh's place in the geometry pool.
    public uint firstIndex;
    public uint indexCount;
    public int vertexOffset;

    // The place of the instance's material in the material table, see Material.slang.
    public uint material;

    // Written to the picking buffer in the editor. -1 for none.
    public int entityId;

    // The instances of one submesh form a batch, which shares draw commands. See Cull.slang.
    public uint batch;

    // For foliage, what its vertices know about their leaves. Null otherwise.
    public LeafVertex* leaves;
};

// The instance a vertex belongs to. A frame draws the instances in a list of places in the instance
// buffer, and drawnIndex is where the vertex's instance stands in that list.
//
// An entry of the list holds the place in its lower 24 bits and, above them, the level of detail the
// instance is drawn in, in sixteenths of a level.
public MeshInstance meshInstance(MeshInstance* instances, uint* visibleInstances, uint drawnIndex)
{
    return instances[visibleInstances[drawnIndex] & 0x00FFFFFF];
}

// The level of detail an instance is drawn in. Whole levels for most meshes. Foliage is drawn in
// levels in between, so that it changes little by little.
public float drawnLevel(uint* visibleInstances, uint drawnIndex)
{
    return float(visibleInstances[drawnIndex] >> 24) / 16.0;
}

// A number from -1 to 1 that is stored in the lower 16 bits.
float fromSnorm16(uint bits)
{
    return max(float(int(bits << 16) >> 16) / 32767.0, -1.0);
}

// Over how many levels a leaf shrinks before it is no longer drawn.
static const float leafFadeLevels = 0.5;

// Where a vertex of a mesh is, in the mesh's own space, at the level the mesh is drawn in. Only foliage
// moves: a coarser level draws fewer leaves, and those are made larger about their centres, so that
// together they cover what all the leaves covered. A leaf that the next level leaves out shrinks away
// before, so no leaf is there in one frame and gone in the next.
//
// vertexIndex is the index the vertex is drawn with (SV_VulkanVertexID), which counts on from where
// the submesh's vertices start in the geometry pool.
public float3 leafPosition(MeshInstance instance, float3 position, uint vertexIndex, float level)
{
    if (level <= 0.0 || instance.leaves == nullptr)
        return position;

    // A piece larger than a leaf is left as it is until the leaves have grown to its size.
    LeafVertex leaf = instance.leaves[int(vertexIndex) - instance.vertexOffset];
    float lastLevel = float((leaf.pivotZAndLevels >> 16) & 0xFF) / 8.0;
    float firstLevel = float(leaf.pivotZAndLevels >> 24) / 8.0;
    if (level <= firstLevel)
        return position;

    float3 fromCentre = float3(fromSnorm16(leaf.pivotXY), fromSnorm16(leaf.pivotXY >> 16), fromSnorm16(leaf.pivotZAndLevels));
    float3 pivot = instance.boundingSphere.xyz + fromCentre * instance.boundingSphere.w;

    // It shrinks away over the last half level it is drawn in, or over all of its levels if those are fewer.
    float shrink = saturate((lastLevel - level) / max(min(lastLevel - firstLevel, leafFadeLevels), 0.0001));

    // Half the leaves at twice the area each: larger by the square root of two per level.
    float grow = sqrt(exp2(level - firstLevel));
    return pivot + (position - pivot) * (grow * shrink);
}

// The semantic to give a vertex shader's instance index: SV_VulkanInstanceID counts on from the first
// instance of the draw call, as the list of drawn instances does. SV_InstanceID would start at zero in every draw.

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
