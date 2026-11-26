// The first step of ambient occlusion, see AmbientOcclusionPass: the scene's depth as distances from
// the camera, in five sizes, each half the one before.
//
// The occlusion looks at places many pixels away from a pixel. Read from the full picture, each of
// those reads would miss the GPU's cache. A place far from the pixel is read from a smaller size
// instead, where one pixel stands for what several of the full picture show.
// After XeGTAO (Intel, 2021, MIT licence), whose way of making the sizes this is.

import Textures;

struct DepthConstants
{
    // The size of the scene's depth buffer in pixels, and where the texture table has it.
    int2 size;
    uint depthTexture;

    // Where the table of writable pictures has each of the five sizes, the largest first.
    uint size0;
    uint size1;
    uint size2;
    uint size3;
    uint size4;

    // The distance of the camera's near plane, which turns a depth into a distance.
    float nearPlane;

    // How far around a place the occlusion looks, in the scene's units.
    float radius;
};
[vk::push_constant] ConstantBuffer<DepthConstants> constants;

// Have to match AmbientOcclusion.slang: the farthest distance the pictures hold, which is the largest
// number a half float does, and the share of the radius over which what occludes fades out.
static const float farthestDistance = 65504.0;
static const float fadeShareOfRadius = 0.615;

// What the threads of a group hand each other from one size to the next.
static groupshared float handedOn[8][8];

// The scene's depth is reversed and has no far plane: 1 at the near plane, towards 0 far away, and
// exactly 0 where nothing is.
float distanceOfDepth(float depth)
{
    return depth > 0.0 ? min(constants.nearPlane / depth, farthestDistance) : farthestDistance;
}

float distanceAt(int2 pixel)
{
    return distanceOfDepth(textures[constants.depthTexture].Load(int3(clamp(pixel, int2(0, 0), constants.size - 1), 0)).r);
}

// One distance for four. Not their average, which would put a surface between a near thing and
// the far wall behind it, where nothing is. And not the farthest, which would lose every thin thing.
// The farthest counts in full, and the others the less the farther in front of it they are, by
// the measure the occlusion itself fades what is far from a place out with.
float oneForFour(float a, float b, float c, float d)
{
    float farthest = max(max(a, b), max(c, d));

    // A smaller size stands for wider surroundings, so the measure is three quarters of the radius.
    float radius = 0.75 * constants.radius;
    float fade = fadeShareOfRadius * radius;
    float fadeFrom = radius * (1.0 - fadeShareOfRadius);

    float4 distances = float4(a, b, c, d);
    float4 weights = saturate((farthest - distances) * (-1.0 / fade) + (fadeFrom / fade + 1.0));
    return dot(weights, distances) / dot(weights, float4(1.0, 1.0, 1.0, 1.0));
}

// A group reaches past the picture's edges, and its threads cannot leave early: they hand each other
// results. So every write checks its place.
void write(uint picture, int level, int2 place, float value)
{
    int2 size = max(constants.size >> level, int2(1, 1));
    if (place.x < size.x && place.y < size.y)
        writableTextures[picture][place] = float4(value, 0.0, 0.0, 0.0);
}

// A thread does two by two pixels of the largest size, so a group of 8 by 8 threads does 16 by 16
// pixels: one pixel of the smallest size. All five sizes are written by one dispatch.
[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID, uint3 threadInGroup : SV_GroupThreadID)
{
    int2 place = int2(thread.xy);
    int2 inGroup = int2(threadInGroup.xy);
    int2 pixel = place * 2;

    float a = distanceAt(pixel);
    float b = distanceAt(pixel + int2(1, 0));
    float c = distanceAt(pixel + int2(0, 1));
    float d = distanceAt(pixel + int2(1, 1));
    write(constants.size0, 0, pixel, a);
    write(constants.size0, 0, pixel + int2(1, 0), b);
    write(constants.size0, 0, pixel + int2(0, 1), c);
    write(constants.size0, 0, pixel + int2(1, 1), d);

    float one = oneForFour(a, b, c, d);
    write(constants.size1, 1, place, one);
    handedOn[inGroup.x][inGroup.y] = one;
    GroupMemoryBarrierWithGroupSync();

    // Every second thread each way puts four of those together, then every fourth, then one.
    if ((inGroup.x & 1) == 0 && (inGroup.y & 1) == 0)
    {
        one = oneForFour(handedOn[inGroup.x][inGroup.y], handedOn[inGroup.x + 1][inGroup.y],
            handedOn[inGroup.x][inGroup.y + 1], handedOn[inGroup.x + 1][inGroup.y + 1]);
        write(constants.size2, 2, place / 2, one);
        handedOn[inGroup.x][inGroup.y] = one;
    }
    GroupMemoryBarrierWithGroupSync();

    if ((inGroup.x & 3) == 0 && (inGroup.y & 3) == 0)
    {
        one = oneForFour(handedOn[inGroup.x][inGroup.y], handedOn[inGroup.x + 2][inGroup.y],
            handedOn[inGroup.x][inGroup.y + 2], handedOn[inGroup.x + 2][inGroup.y + 2]);
        write(constants.size3, 3, place / 4, one);
        handedOn[inGroup.x][inGroup.y] = one;
    }
    GroupMemoryBarrierWithGroupSync();

    if (inGroup.x == 0 && inGroup.y == 0)
    {
        one = oneForFour(handedOn[0][0], handedOn[4][0], handedOn[0][4], handedOn[4][4]);
        write(constants.size4, 4, place / 8, one);
    }
}
