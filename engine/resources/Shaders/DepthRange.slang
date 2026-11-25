// Finds the nearest and the farthest depth of the scene's finished depth buffer, the sky left out.
// The CPU reads the two a few frames later and knows which stretch of the view holds anything,
// which is what the cascades of the sun's shadow are shared out over. See DepthRangePass.
//
// The scene's depth is reversed: 1 at the near plane, towards 0 far away, and exactly 0 where nothing is.

import Textures;

// Depths as the bits of their floats. For numbers that are not negative, the larger float has the
// larger bits, so whole numbers can be compared in their place, which is what the GPU can do atomically.
// Has to match DepthRange in DepthRangePass.cpp.
struct DepthRange
{
    uint nearest;
    uint farthest;
};

struct RangeConstants
{
    // Starts every frame as the widest range there is the wrong way round: nearest 0, farthest 1.
    DepthRange* range;

    // The size of the depth buffer in pixels, and where the texture table has it.
    int2 size;
    uint depthTexture;
};
[vk::push_constant] ConstantBuffer<RangeConstants> constants;

// One thread looks at a square of this many pixels a side, and adds what it finds to the range once.
// Has to match DepthRangePass.cpp, as does the size of a group.
static const int tileSize = 8;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    int2 first = int2(thread.xy) * tileSize;
    if (first.x >= constants.size.x || first.y >= constants.size.y)
        return;
    int2 end = min(first + tileSize, constants.size);

    Texture2D depthBuffer = textures[constants.depthTexture];
    float nearest = 0.0;
    float farthest = 1.0;
    for (int y = first.y; y < end.y; ++y)
    {
        for (int x = first.x; x < end.x; ++x)
        {
            float depth = depthBuffer.Load(int3(x, y, 0)).r;
            if (depth > 0.0)
            {
                nearest = max(nearest, depth);
                farthest = min(farthest, depth);
            }
        }
    }

    // A square of sky adds nothing.
    if (nearest > 0.0)
    {
        InterlockedMax(constants.range[0].nearest, asuint(nearest));
        InterlockedMin(constants.range[0].farthest, asuint(farthest));
    }
}
