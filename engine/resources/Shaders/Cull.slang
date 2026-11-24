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

import Scene;
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

    // What the sun sees of each cascade of its shadow.
    View sun[shadowCascadeCount];

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

    // What is smaller than this many texels of a cascade's map casts no shadow into it.
    float smallestCasterInTexels;

    // The world as the sun sees it, turned but not moved, and per cascade the square of ground its map
    // covers in that view: the middle in xy, half its width in z, and in w the size of one of its texels.
    float4x4 sunView;
    float4 cascades[shadowCascadeCount];
};

// Where the list of levels has those of a batch. No levels for a batch that is not drawn from here.
// Has to match BatchLods in GpuScene.cpp.
struct BatchLods
{
    uint first;
    uint count;

    // For foliage: the size of a leaf, in the units a level's error is in. Zero for everything else.
    float leafSize;
    uint padding;
};

// One level of detail of a batch: how far its surface is from the original's at most, in the mesh's
// units, and the draw commands its instances are counted into, the camera's for the two phases and the
// sun's. The levels of a batch get coarser one by one. Has to match LodCommands in GpuScene.cpp.
struct LodCommands
{
    float error;
    uint camera;
    uint cameraLate;
    uint sun[shadowCascadeCount];
    uint padding;
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

// Levels are counted in sixteenths here, so that foliage can be drawn between two levels.
static const uint levelSteps = 16;

// The coarsest level of a batch whose error cannot be seen from a distance: it stays under the pixels
// allowed. In sixteenths of a level. A mesh is drawn in whole levels. Foliage is drawn in the whole level
// below the one returned, and its leaves grow and shrink by what is left over, see leafPosition in Mesh.slang.
uint levelAtDistance(BatchLods batchLods, float distance, float scale)
{
    uint coarsest = batchLods.count - 1;
    if (constants.frame.forcedLevel >= 0)
        return min(uint(constants.frame.forcedLevel), coarsest) * levelSteps;
    if (constants.frame.isPerspective == 0)
        return 0;

    // Foliage: level k has leaves larger by the square root of 2^k, and is fine while those are small
    // enough on screen. So the level grows by two every time the distance doubles.
    if (batchLods.leafSize > 0.0)
    {
        float firstCoarserAt = max(batchLods.leafSize * scale * constants.frame.lodScale, 0.000001);
        float level = 2.0 * log2(distance / firstCoarserAt);
        return uint(clamp(level, 0.0, float(coarsest)) * float(levelSteps));
    }

    uint level = 0;
    while (level < coarsest && constants.lods[batchLods.first + level + 1].error * scale * constants.frame.lodScale <= distance)
        ++level;
    return level * levelSteps;
}

// How many texels of a shadow map an error may be large: a shadow cannot show more than its map holds.
static const float shadowErrorInTexels = 1.0;

// The level an instance is drawn in for one cascade of the sun's shadow. It is at least as coarse as the
// level the camera sees it in, by the shadow bias, and coarser if the cascade's map could not show the
// difference: an error smaller than a texel of the map is lost in it. So a far cascade, whose texels are
// large, draws coarse levels of everything, however near the camera it stands.
uint shadowLevel(BatchLods batchLods, uint cascade, float distanceToCamera, float scale)
{
    // The distance at which an error as large as the texels allowed would be as large as the pixels allowed.
    float asIfAt = constants.frame.cascades[cascade].w * shadowErrorInTexels * constants.frame.lodScale;
    uint level = levelAtDistance(batchLods, max(distanceToCamera, asIfAt), scale);
    return min(level + constants.frame.shadowBias * levelSteps, (batchLods.count - 1) * levelSteps);
}

// True if a cascade does not need the shadow of an instance, because the cascade before it shows all of
// it. A place takes its shadow from the first cascade whose map it lies in. A shadow falls along the
// sun's direction, so everything it falls on lies, as the sun sees it, where its caster lies. If that is
// well inside the square the cascade before covers, inside the part that is not mixed with this one,
// then nothing that looks into this cascade's map can be in that shadow.
bool isCoveredByNearerCascade(uint cascade, float3 centre, float radius)
{
    if (cascade == 0)
        return false;

    float4 nearer = constants.frame.cascades[cascade - 1];

    // A few texels are left for the softening of the shadow's edge and for looking it up off the surface.
    float unmixed = nearer.z * (1.0 - shadowCascadeBlend) - 4.0 * nearer.w;
    float2 fromMiddle = abs(mul(constants.frame.sunView, float4(centre, 1.0)).xy - nearer.xy);
    return max(fromMiddle.x, fromMiddle.y) + radius <= unmixed;
}

// Adds an instance to a draw command. The level it is drawn in goes into the list with its place,
// where the vertex shaders find it.
void draw(uint command, uint place, uint level)
{
    uint countBefore;
    InterlockedAdd(constants.commands[command].instanceCount, 1, countBefore);
    constants.visibleInstances[constants.commands[command].firstInstance + countBefore] = place | (level << 24);
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

    // The distance from the camera to the point of the instance that is nearest to it.
    float distanceToCamera = max(length(mul(constants.frame.view, float4(centre, 1.0)).xyz) - radius, constants.frame.nearPlane);

    // Both phases pick the same level, so an instance is drawn in one level per frame.
    uint level = levelAtDistance(batchLods, distanceToCamera, scale);
    LodCommands batchCommands = constants.lods[batchLods.first + level / levelSteps];

    if (batchCommands.camera != noCommand)
    {
        bool inView = isSeen(constants.frame.camera, centre, radius);
        bool seenLastFrame = constants.visibility[place] != 0;

        if (constants.phase == phaseEarly)
        {
            if (inView && seenLastFrame)
                draw(batchCommands.camera, place, level);
        }
        else
        {
            bool seen = inView && !isHidden(centre, radius);
            if (seen && !seenLastFrame)
                draw(batchCommands.cameraLate, place, level);

            constants.visibility[place] = seen ? 1 : 0;
        }
    }

    // Shadows are not culled against other things, so the sun's views need one phase only. The maps of
    // the cascades lie inside each other, so an instance reaches into several: it is drawn into those
    // that need its shadow, each in the level that the map can still show.
    if (constants.phase == phaseEarly)
    {
        for (uint cascade = 0; cascade < shadowCascadeCount; ++cascade)
        {
            uint levelInShadow = shadowLevel(batchLods, cascade, distanceToCamera, scale);
            uint command = constants.lods[batchLods.first + levelInShadow / levelSteps].sun[cascade];
            if (command == noCommand || !isSeen(constants.frame.sun[cascade], centre, radius))
                continue;
            if (isCoveredByNearerCascade(cascade, centre, radius))
                continue;

            // Too small for this cascade's map to show more of it than a blur.
            if (radius < constants.frame.smallestCasterInTexels * constants.frame.cascades[cascade].w)
                continue;

            draw(command, place, levelInShadow);
        }
    }
}
