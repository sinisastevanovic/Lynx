// Puts the picture together from several frames. See TemporalPass.
//
// For now: the average of the frames of a view that stands still. With the camera's jitter on, every
// frame samples the pixels in another place, and their average is the picture with that many samples
// a pixel.

import Textures;

struct TemporalConstants
{
    // The size of the pictures in pixels.
    int2 size;

    // Where the texture table has the frame's picture and the result of the last frame, and where
    // the table of writable pictures has the one this frame's result goes into.
    uint sceneTexture;
    uint historyTexture;
    uint outputTexture;

    // How much the frame counts in the result, the last frame's result counting the rest.
    float currentWeight;
};
[vk::push_constant] ConstantBuffer<TemporalConstants> constants;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    // The last groups reach past the picture's edges.
    int2 pixel = int2(thread.xy);
    if (pixel.x >= constants.size.x || pixel.y >= constants.size.y)
        return;

    float3 current = textures[constants.sceneTexture].Load(int3(pixel, 0)).rgb;
    float3 history = textures[constants.historyTexture].Load(int3(pixel, 0)).rgb;
    writableTextures[constants.outputTexture][pixel] = float4(lerp(history, current, constants.currentWeight), 1.0);
}
