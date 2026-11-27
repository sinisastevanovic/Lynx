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

    // For a plant, which moves in the wind: how high it is above where it stands and how far it
    // reaches sideways from there, in the mesh's own space. Zero for everything else.
    public float swayHeight;
    public float swayReach;
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

// A vertex of a mesh in the mesh's own space.
public struct LeafPlace
{
    // Where the vertex is, at the level the mesh is drawn in.
    public float3 position;

    // The centre of the leaf the vertex is part of. The vertex itself where there are no leaves.
    public float3 pivot;
};

// Where a vertex of a mesh is, in the mesh's own space, at the level the mesh is drawn in. Only foliage
// moves: a coarser level draws fewer leaves, and those are made larger about their centres, so that
// together they cover what all the leaves covered. A leaf that the next level leaves out shrinks away
// before, so no leaf is there in one frame and gone in the next.
//
// vertexIndex is the index the vertex is drawn with (SV_VulkanVertexID), which counts on from where
// the submesh's vertices start in the geometry pool.
public LeafPlace leafPlace(MeshInstance instance, float3 position, uint vertexIndex, float level)
{
    LeafPlace place;
    place.position = position;
    place.pivot = position;

    // The wind needs the centre of a leaf at every level, the levels only from the first coarser one on.
    bool isInWind = instance.swayHeight > 0.0 && scene.wind.z > 0.0;
    if (instance.leaves == nullptr || (level <= 0.0 && !isInWind))
        return place;

    LeafVertex leaf = instance.leaves[int(vertexIndex) - instance.vertexOffset];
    float lastLevel = float((leaf.pivotZAndLevels >> 16) & 0xFF) / 8.0;
    float firstLevel = float(leaf.pivotZAndLevels >> 24) / 8.0;

    float3 fromCentre = float3(fromSnorm16(leaf.pivotXY), fromSnorm16(leaf.pivotXY >> 16), fromSnorm16(leaf.pivotZAndLevels));
    float3 pivot = instance.boundingSphere.xyz + fromCentre * instance.boundingSphere.w;
    place.pivot = pivot;
    if (level <= 0.0)
        return place;

    // The leaf shrinks away over the last half level it is drawn in, or over all of its levels if those are fewer.
    float shrink = saturate((lastLevel - level) / max(min(lastLevel, leafFadeLevels), 0.0001));

    // Half the leaves at twice the area each: larger by the square root of two per level. A vertex that
    // lies farther from the centre of its piece than a leaf reaches only joins in at its own first level,
    // when the leaves have grown out to it. A piece longer than a leaf would otherwise stick out of
    // the crown as a spike.
    float grow = sqrt(exp2(max(level - firstLevel, 0.0)));
    place.position = pivot + (position - pivot) * (grow * shrink);
    return place;
}

// The semantic to give a vertex shader's instance index: SV_VulkanInstanceID counts on from the first
// instance of the draw call, as the list of drawn instances does. SV_InstanceID would start at zero in every draw.

// Where a vertex is in the world and on the screen.
public struct MeshPosition
{
    public float4 world;
    public float4 clip;
};

// A direction along the surface, such as a tangent, turned and scaled with the instance. Not normalised.
public float3 meshDirection(float4x4 model, float3 direction)
{
    return mul((float3x3)model, direction);
}

// How much of each part of the motion a wind of a speed brings about, from 0 to under 1. The force
// on a whole plant grows with the square of the speed and levels off in a gale: a light breeze does
// not move a trunk. Branches and leaves answer to far less.
float windOnPlant(float speed)
{
    return speed * speed / (speed * speed + 225.0);
}

float windOnBranches(float speed)
{
    return speed / (speed + 10.0);
}

float windOnLeaves(float speed)
{
    return speed / (speed + 5.0);
}

// The farthest each part moves a vertex in the strongest wind: the plant as a share of its height,
// a branch as a share of the plant's reach, and the angle a leaf rocks by. Together they stay under
// LargestSwayShare in GpuScene.h, which the CPU allows for.
static const float plantBend = 0.1;
static const float branchSway = 0.035;
static const float leafRock = 0.25;

// The longest piece that rocks by the whole angle, in the mesh's units. A longer one, like a twig,
// is turned by less, or its end would leave the branch it grows from.
static const float longestLeaf = 0.15;

// Beyond this distance from the camera nothing moves in the wind, and leaves stop to flutter beyond
// the second. The motion fades out towards both. What a tree moves by is under two pixels out there,
// and what stands still costs its vertices nothing and its shadow is not drawn anew in the eye.
static const float windReach = 220.0;
static const float flutterReach = 60.0;

static const float twoPi = 6.283185307179586;

// By how much the sphere around an instance has to grow to hold it in the wind, for culling.
// scale is how much larger the instance is than its mesh.
public float swayMargin(MeshInstance instance, float scale)
{
    float speed = scene.wind.z;
    if (instance.swayHeight <= 0.0 || speed <= 0.0)
        return 0.0;

    // The most each part can add in this wind, see windOn: the lean with its swing to the side,
    // a branch's lift with its give, and a leaf's rocking at the end of the longest arm that rocks fully.
    float gusts = 1.0 + scene.wind.w;
    float plant = plantBend * windOnPlant(speed) * min(gusts, 1.2) * 1.05 * instance.swayHeight;
    float branches = branchSway * windOnBranches(speed) * gusts * 1.12 * instance.swayReach;
    float leaves = leafRock * 1.06 * longestLeaf;
    return (plant + branches + leaves) * scale;
}

// Where the wind takes a vertex of a plant: given where it is in the world in still air, where it is
// at the given time, one of scene.windTime. Everything that is not a plant stays where it is.
//
// Nothing about the wind is stored with a mesh. The motion is made of three parts, worked out from
// where the vertex is in the plant, as in Crysis (Sousa, GPU Gems 3) and Horizon Zero Dawn (Gilbert
// and Sanders, GDC 2017):
// - The whole plant leans with the wind and swings at a pace of its own, more the higher up, with
//   its length kept, so that it bends and does not shear.
// - Branches move up and down, more the farther out from the trunk, in waves that run through the crown.
// - A leaf rocks about its own centre. It takes the first two parts from its centre, so that it
//   moves in one piece.
// The wind itself is one speed that rises and falls in gusts: waves that travel with the wind and
// are read once per plant, where it stands, so a gust is seen to run over a forest.
public float3 windOn(MeshInstance instance, float4x4 model, LeafPlace place, float3 world, float time)
{
    float speed = scene.wind.z;
    if (instance.swayHeight <= 0.0 || speed <= 0.0)
        return world;

    float3 root = float3(model[0][3], model[1][3], model[2][3]);
    float fromCamera = distance(root, scene.cameraPosition.xyz);
    float near = saturate((windReach - fromCamera) / (0.25 * windReach));
    if (near <= 0.0)
        return world;

    float2 direction = scene.wind.xy;
    float2 across = float2(-direction.y, direction.x);
    float scale = length(meshDirection(model, float3(0.0, 1.0, 0.0)));

    // The gusts: two waves of 37 and 88 units, slanted against each other, between -1 and 1.
    float along = dot(root.xz, direction) - speed * time;
    float sideways = dot(root.xz, across);
    float gust = 0.5 * sin(along * 0.17 + sideways * 0.05) + 0.5 * sin(along * 0.071 - sideways * 0.09 + 2.0);
    float pressure = 1.0 + scene.wind.w * gust;

    // Every plant is at another point of its swinging.
    float plantPhase = frac(dot(root.xz, float2(0.137, 0.291))) * twoPi;

    float3 leafArm = meshDirection(model, place.position - place.pivot);
    float3 fromRoot = world - leafArm - root;
    float3 inMesh = place.pivot;
    float height = saturate(inMesh.y / instance.swayHeight);

    // The whole plant. A small one swings faster than a large one.
    float worldHeight = instance.swayHeight * scale;
    float pace = 2.5 * rsqrt(max(worldHeight, 1.0) * 0.1);
    float lean = min(pressure * (0.6 + 0.4 * sin(time * pace + plantPhase)), 1.2);
    float swing = 0.3 * sin(time * pace * 0.83 + plantPhase * 1.7);
    float bend = plantBend * windOnPlant(speed) * near * height * height * worldHeight;
    float2 push = (direction * lean + across * swing) * bend;
    float3 bent = fromRoot + float3(push.x, 0.0, push.y);
    bent *= sqrt(dot(fromRoot, fromRoot) / max(dot(bent, bent), 1e-8));

    // The branches. Not the foot of the trunk, however wide it is.
    float outwards = saturate(length(inMesh.xz) / instance.swayReach);
    float branch = outwards * outwards * saturate(height * 4.0);
    float branchPhase = plantPhase + dot(inMesh, float3(1.3, 0.7, 1.9)) * (3.0 / instance.swayReach);
    float lift = (sin(time * 5.1 + branchPhase) + 0.5 * sin(time * 11.3 + branchPhase * 2.3)) / 1.5;
    float give = 0.5 * sin(time * 4.3 + branchPhase * 1.3 + 1.0);
    float sway = branchSway * windOnBranches(speed) * pressure * near * branch * instance.swayReach * scale;
    bent += float3(direction.x * give, lift, direction.y * give) * sway;

    // The leaf.
    float armLength = length(leafArm);
    float flutterNear = saturate((flutterReach - fromCamera) / (0.5 * flutterReach));
    if (armLength > 0.0 && flutterNear > 0.0)
    {
        float leafPhase = frac(sin(dot(place.pivot, float3(12.9898, 78.233, 37.719))) * 43758.5453) * twoPi;
        float rock = (sin(time * 19.0 + leafPhase) + 0.5 * sin(time * 31.0 + leafPhase * 1.9)) / 1.5;
        float angle = leafRock * windOnLeaves(speed) * min(pressure, 1.5) * flutterNear * rock * saturate(longestLeaf * scale / armLength);
        float3 axis = float3(across.x, 0.35 * sin(leafPhase * 3.0), across.y);
        leafArm += angle * cross(axis, leafArm);
    }

    return root + bent + leafArm;
}

// Where a vertex is in the world, in this frame's wind.
public float3 meshWorld(MeshInstance instance, float4x4 model, LeafPlace place)
{
    float3 world = mul(model, float4(place.position, 1.0)).xyz;
    return windOn(instance, model, place, world, scene.windTime.x);
}

// The depth pass and the forward pass both go through here, so that they arrive at exactly the same
// position: the forward pass draws only where the depth equals what the depth pass wrote.
public MeshPosition meshPosition(MeshInstance instance, float4x4 model, LeafPlace place)
{
    MeshPosition result;
    result.world = float4(meshWorld(instance, model, place), 1.0);
    result.clip = mul(scene.viewProjection, result.world);
    return result;
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
