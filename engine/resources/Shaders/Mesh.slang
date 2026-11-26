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

// Where an instance is in the world: the first three rows of its matrix. The fourth is always 0, 0, 0, 1.
// Kept apart from the rest of the instance, because it is what changes when something moves, and kept
// twice: in a buffer for this frame and in one for the frame before. Has to match InstanceTransform in GpuScene.h.
public struct MeshTransform
{
    public float4 rows[3];
};

// One submesh of one mesh in the scene. The instances of the whole scene are in one buffer that stays
// on the GPU from frame to frame. Has to match GpuInstance in GpuScene.h.
public struct MeshInstance
{
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

// How large a scattered instance is, against its mesh. Has to match ScatterInstance::Size in Scatter.h.
public float scatterSize(ScatterInstance instance)
{
    return exp2((float((instance.packed >> 12) & 0xFF) - 128.0) / 64.0);
}

// Which kind of thing a scattered instance is: its number in the list of kinds.
public uint scatterKind(ScatterInstance instance)
{
    return instance.packed >> 20;
}

// The matrix that takes the mesh of a scattered instance into the world: made larger or smaller, turned
// about the upward axis and moved to where the instance stands.
public float4x4 scatterModel(ScatterInstance instance)
{
    float angle = float(instance.packed & 0xFFF) * (6.283185307179586 / 4096.0);
    float size = scatterSize(instance);
    float alongX = size * cos(angle);
    float alongZ = size * sin(angle);
    return float4x4(
        float4(alongX, 0.0, alongZ, instance.position.x),
        float4(0.0, size, 0.0, instance.position.y),
        float4(-alongZ, 0.0, alongX, instance.position.z),
        float4(0.0, 0.0, 0.0, 1.0));
}

// A frame draws its instances in a list, and a vertex shader's instance index (SV_VulkanInstanceID) says
// where the vertex's instance stands in it. An entry of the list is two numbers. For an instance with a
// place in the instance buffer, the first is the place. For a scattered instance it is where the instance
// stands among the scattered ones, and the second holds, above its lowest byte, the place that keeps
// what all scattered instances of the submesh share, plus one. The lowest byte of the second is the
// level of detail the instance is drawn in, in sixteenths of a level. Has to match GpuScene::DrawnEntry.
//
// A vertex shader reads its entry once and asks the functions below about it.
public uint2 drawnEntry(uint2* visibleInstances, uint drawnIndex)
{
    return visibleInstances[drawnIndex];
}

// The instance of an entry: for a scattered one, what it shares with the others of its submesh.
public MeshInstance meshInstance(MeshInstance* instances, uint2 entry)
{
    uint sharedPlace = entry.y >> 8;
    return instances[sharedPlace != 0 ? sharedPlace - 1 : entry.x];
}

// The matrix that takes a mesh into the world.
public float4x4 meshModel(MeshTransform transform)
{
    return float4x4(transform.rows[0], transform.rows[1], transform.rows[2], float4(0.0, 0.0, 0.0, 1.0));
}

// The same for the instance of an entry.
public float4x4 meshModel(MeshTransform* transforms, uint2 entry)
{
    if ((entry.y >> 8) != 0)
        return scatterModel(scene.scatterInstances[entry.x]);
    return meshModel(transforms[entry.x]);
}

// The matrix the instance of an entry had in the last frame, given its matrix of this frame.
// A scattered instance stands where it stood, so its matrix is not worked out a second time.
public float4x4 previousMeshModel(MeshTransform* previousTransforms, uint2 entry, float4x4 model)
{
    if ((entry.y >> 8) != 0)
        return model;
    return meshModel(previousTransforms[entry.x]);
}

// The level of detail an instance is drawn in. Whole levels for most meshes. Foliage is drawn in
// levels in between, so that it changes little by little.
public float drawnLevel(uint2 entry)
{
    return float(entry.y & 0xFF) / 16.0;
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

    LeafVertex leaf = instance.leaves[int(vertexIndex) - instance.vertexOffset];
    float lastLevel = float((leaf.pivotZAndLevels >> 16) & 0xFF) / 8.0;
    float firstLevel = float(leaf.pivotZAndLevels >> 24) / 8.0;

    float3 fromCentre = float3(fromSnorm16(leaf.pivotXY), fromSnorm16(leaf.pivotXY >> 16), fromSnorm16(leaf.pivotZAndLevels));
    float3 pivot = instance.boundingSphere.xyz + fromCentre * instance.boundingSphere.w;

    // The leaf shrinks away over the last half level it is drawn in, or over all of its levels if those are fewer.
    float shrink = saturate((lastLevel - level) / max(min(lastLevel, leafFadeLevels), 0.0001));

    // Half the leaves at twice the area each: larger by the square root of two per level. A vertex that
    // lies farther from the centre of its piece than a leaf reaches only joins in at its own first level,
    // when the leaves have grown out to it. A piece longer than a leaf would otherwise stick out of
    // the crown as a spike.
    float grow = sqrt(exp2(max(level - firstLevel, 0.0)));
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
public MeshPosition meshPosition(float4x4 model, float3 position)
{
    MeshPosition result;
    result.world = mul(model, float4(position, 1.0));
    result.clip = mul(scene.viewProjection, result.world);
    return result;
}

// A direction along the surface, such as a tangent, turned and scaled with the instance. Not normalised.
public float3 meshDirection(float4x4 model, float3 direction)
{
    return mul((float3x3)model, direction);
}

// A normal of the mesh in the world. Not normalised.
//
// A normal cannot be transformed like a direction along the surface: on an object that is stretched
// along one axis, it would lean the wrong way. What keeps it at a right angle to the surface is the
// matrix of cross products of the model's axes, which costs three cross products instead of a full inverse.
public float3 meshNormal(float4x4 model, float3 normal)
{
    float3 axisX = meshDirection(model, float3(1.0, 0.0, 0.0));
    float3 axisY = meshDirection(model, float3(0.0, 1.0, 0.0));
    float3 axisZ = meshDirection(model, float3(0.0, 0.0, 1.0));

    float3 transformed = cross(axisY, axisZ) * normal.x + cross(axisZ, axisX) * normal.y + cross(axisX, axisY) * normal.z;

    // A mirrored instance turns the cross products around.
    return dot(axisX, cross(axisY, axisZ)) < 0.0 ? -transformed : transformed;
}
