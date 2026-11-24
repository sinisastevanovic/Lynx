// Decides which mesh instances a frame draws, and how finely: every instance of the scene is tested
// against the camera's view and the sun's, and those that are seen are added to the draw command of
// their batch and of the level of detail that suits their distance.
//
// The CPU has laid the frame out: one draw command per batch, level and view that draws nothing yet,
// each with a run in the list of drawn instances that could hold the whole batch. Here an instance that is seen
// takes the next free place in the run and the command's count goes up by one. Many instances do that
// at the same time, so the count is raised atomically, which also hands every instance a place of its own.
// The order of the instances within a run therefore differs from frame to frame.
//
// For the camera this runs twice per frame, so that what lies behind other things is not drawn
// (after "GPU-Driven Rendering Pipelines", Haar and Aaltonen, SIGGRAPH 2015):
//   Early: draws what the camera saw in the last frame and still has in view. Nearly everything that
//     is seen in this frame is among it. The depth of that becomes the depth pyramid.
//   Late: tests every instance against the pyramid. What is not hidden and was not drawn early is drawn
//     now, and what was seen is remembered for the next frame.
// Nothing that is visible is ever left out this way: the early phase may draw too much, the late phase
// draws whatever is missing.

import Mesh;
import Textures;

// The planes around what a view sees, pointing inwards: the normal in xyz, the distance in w.
// Has to match CullView in GpuScene.cpp.
struct View
{
    float4 planes[6];
};

// What culling knows of the frame. Has to match CullFrame in GpuScene.cpp.
struct CullFrame
{
    View camera;
    View sun;

    // Takes a position in the world to the camera's space, where the camera looks along -z.
    float4x4 view;

    // How much the camera's projection scales x and y, and the distance of its near plane.
    float projectionX;
    float projectionY;
    float nearPlane;

    // Only a perspective is tested against the depth pyramid, and only a perspective picks levels of detail.
    uint isPerspective;

    // A level is fine enough for an instance at a distance of at least its error times this.
    float lodScale;

    // The level every instance is drawn in. Negative for the level its distance picks.
    int forcedLevel;

    // How many levels coarser the sun's view draws.
    uint shadowBias;
    uint padding;
};

// Where the list of levels has those of a batch. No levels for a batch that is not drawn from here.
// Has to match BatchLods in GpuScene.cpp.
struct BatchLods
{
    uint first;
    uint count;
};

// One level of detail of a batch: how far its surface is from the original's at most, in the mesh's
// units, and the draw commands its instances are counted into, the camera's for the two phases and the
// sun's. The levels of a batch get coarser one by one. Has to match LodCommands in GpuScene.cpp.
struct LodCommands
{
    float error;
    uint camera;
    uint cameraLate;
    uint sun;
};

// What a draw call reads from a buffer, as Vulkan defines it (VkDrawIndexedIndirectCommand).
struct DrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

static const uint noBatch = 0xFFFFFFFF;
static const uint noCommand = 0xFFFFFFFF;

// Has to match CullPhase in RenderPass.h.
static const uint phaseEarly = 0;
static const uint phaseLate = 1;

struct CullConstants
{
    MeshInstance* instances;
    uint* visibleInstances;
    DrawCommand* commands;
    BatchLods* batches;
    LodCommands* lods;
    CullFrame* frame;

    // Per place, whether the camera saw its instance in the last frame.
    uint* visibility;

    // How many places the instance buffer has. Some of them may be empty.
    uint placeCount;
    uint phase;

    // The depth pyramid: where the texture table has it, how many sizes it has and its largest size
    // in pixels. No sizes means there is no pyramid, and nothing counts as hidden.
    uint pyramidTexture;
    uint pyramidMipCount;
    int2 pyramidSize;
};
[vk::push_constant] ConstantBuffer<CullConstants> constants;

// False only if the sphere lies wholly behind one of the planes.
bool isSeen(View view, float3 centre, float radius)
{
    for (uint plane = 0; plane < 6; ++plane)
    {
        if (dot(view.planes[plane].xyz, centre) + view.planes[plane].w < -radius)
            return false;
    }
    return true;
}

// The rectangle a sphere covers on screen, as texture coordinates: the smallest corner in xy, the
// largest in zw. The sphere is given in the camera's space with z as the distance in front of the camera,
// and has to lie wholly in front of the near plane.
// From "2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere" (Mara and McGuire, 2013).
float4 screenBounds(float3 centre, float radius, float projectionX, float projectionY)
{
    float3 scaled = centre * radius;
    float distanceSquared = centre.z * centre.z - radius * radius;

    float lengthX = sqrt(centre.x * centre.x + distanceSquared);
    float minX = (lengthX * centre.x - scaled.z) / (lengthX * centre.z + scaled.x);
    float maxX = (lengthX * centre.x + scaled.z) / (lengthX * centre.z - scaled.x);

    float lengthY = sqrt(centre.y * centre.y + distanceSquared);
    float minY = (lengthY * centre.y - scaled.z) / (lengthY * centre.z + scaled.y);
    float maxY = (lengthY * centre.y + scaled.z) / (lengthY * centre.z - scaled.y);

    // Up in the camera's space is the top of the screen, where the texture coordinate is 0.
    return float4(minX * projectionX, -maxY * projectionY, maxX * projectionX, -minY * projectionY) * 0.5 + 0.5;
}

// True if everything the early phase drew in front of the sphere covers it completely.
bool isHidden(float3 centre, float radius)
{
    if (constants.pyramidMipCount == 0 || constants.frame.isPerspective == 0)
        return false;

    float3 inView = mul(constants.frame.view, float4(centre, 1.0)).xyz;
    inView.z = -inView.z;

    // A sphere that reaches the near plane has no rectangle on screen. It is close enough to be seen.
    float nearPlane = constants.frame.nearPlane;
    if (inView.z - radius <= nearPlane)
        return false;

    float4 bounds = saturate(screenBounds(inView, radius, constants.frame.projectionX, constants.frame.projectionY));

    // The size of the pyramid in which the rectangle is at most one pixel wide and high. There it
    // touches at most two pixels in each direction, so the four at its corners cover it.
    float2 sizeInPixels = (bounds.zw - bounds.xy) * float2(constants.pyramidSize);
    int level = clamp(int(ceil(log2(max(max(sizeInPixels.x, sizeInPixels.y), 1.0)))), 0, int(constants.pyramidMipCount) - 1);

    int2 levelSize = max(constants.pyramidSize >> level, int2(1, 1));
    int2 first = clamp(int2(bounds.xy * float2(levelSize)), int2(0, 0), levelSize - 1);
    int2 last = clamp(int2(bounds.zw * float2(levelSize)), int2(0, 0), levelSize - 1);

    Texture2D pyramid = textures[constants.pyramidTexture];
    float farthest = min(
        min(pyramid.Load(int3(first.x, first.y, level)).r, pyramid.Load(int3(last.x, first.y, level)).r),
        min(pyramid.Load(int3(first.x, last.y, level)).r, pyramid.Load(int3(last.x, last.y, level)).r));

    // Depth is reversed: the near plane is 1 and larger means nearer. The sphere is hidden if the
    // farthest thing drawn over its rectangle is still nearer than the sphere's nearest point.
    float nearestOfSphere = nearPlane / (inView.z - radius);
    return farthest > nearestOfSphere;
}

// The coarsest level of a batch whose error cannot be seen at the instance's distance: it stays under
// the pixels allowed even at the point of the instance that is nearest to the camera.
uint levelOfDetail(BatchLods batchLods, float3 centre, float radius, float scale)
{
    uint coarsest = batchLods.count - 1;
    if (constants.frame.forcedLevel >= 0)
        return min(uint(constants.frame.forcedLevel), coarsest);
    if (constants.frame.isPerspective == 0)
        return 0;

    float distanceToCamera = length(mul(constants.frame.view, float4(centre, 1.0)).xyz);
    float nearest = max(distanceToCamera - radius, constants.frame.nearPlane);

    uint level = 0;
    while (level < coarsest && constants.lods[batchLods.first + level + 1].error * scale * constants.frame.lodScale <= nearest)
        ++level;
    return level;
}

void draw(uint command, uint place)
{
    uint countBefore;
    InterlockedAdd(constants.commands[command].instanceCount, 1, countBefore);
    constants.visibleInstances[constants.commands[command].firstInstance + countBefore] = place;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    // The last group reaches past the last place.
    uint place = thread.x;
    if (place >= constants.placeCount)
        return;

    uint batch = constants.instances[place].batch;
    if (batch == noBatch)
        return;

    // A batch without levels is see-through: the CPU culls and sorts those.
    BatchLods batchLods = constants.batches[batch];
    if (batchLods.count == 0)
        return;

    // The sphere around the instance in the world. Its radius grows by the largest of the three scales,
    // which keeps it around the mesh however unevenly the instance is scaled.
    MeshInstance instance = constants.instances[place];
    float3 axisX = meshDirection(instance, float3(1.0, 0.0, 0.0));
    float3 axisY = meshDirection(instance, float3(0.0, 1.0, 0.0));
    float3 axisZ = meshDirection(instance, float3(0.0, 0.0, 1.0));
    float scale = sqrt(max(dot(axisX, axisX), max(dot(axisY, axisY), dot(axisZ, axisZ))));
    float3 centre = mul(instance.model, float4(instance.boundingSphere.xyz, 1.0)).xyz;
    float radius = instance.boundingSphere.w * scale;

    // Both phases pick the same level, so an instance is drawn in one level per frame. The shadow may be
    // coarser: it is seen less sharply than the thing itself.
    uint level = levelOfDetail(batchLods, centre, radius, scale);
    LodCommands batchCommands = constants.lods[batchLods.first + level];
    uint shadowLevel = min(level + constants.frame.shadowBias, batchLods.count - 1);
    uint sunCommand = constants.lods[batchLods.first + shadowLevel].sun;

    if (batchCommands.camera != noCommand)
    {
        bool inView = isSeen(constants.frame.camera, centre, radius);
        bool seenLastFrame = constants.visibility[place] != 0;

        if (constants.phase == phaseEarly)
        {
            if (inView && seenLastFrame)
                draw(batchCommands.camera, place);
        }
        else
        {
            bool seen = inView && !isHidden(centre, radius);
            if (seen && !seenLastFrame)
                draw(batchCommands.cameraLate, place);

            constants.visibility[place] = seen ? 1 : 0;
        }
    }

    // Shadows are not culled against other things, so the sun's view needs one phase only.
    if (constants.phase == phaseEarly && sunCommand != noCommand && isSeen(constants.frame.sun, centre, radius))
        draw(sunCommand, place);
}
