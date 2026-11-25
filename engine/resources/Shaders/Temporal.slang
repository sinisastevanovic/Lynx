// Puts the picture together from several frames. See TemporalPass.
//
// Resolving is anti-aliasing over time: the camera is shifted by a part of a pixel in every frame, and
// every pixel mixes the frame's picture with what the frames before it left there, the history. The
// history is fetched from where the pixel's surface was a frame ago, kept within the colours the frame
// has around the pixel, and dropped where the surface was hidden then.
// After "High Quality Temporal Supersampling" (Karis, 2014) and "Filmic SMAA" (Jimenez, 2016).
//
// Accumulating is the plain average of the frames of a view that stands still: with the jitter, the
// picture with that many samples a pixel, which the resolve is measured against.

import Scene;
import Textures;

// Have to match TemporalPass.cpp.
static const uint modeAccumulate = 0;
static const uint modeResolve = 1;

static const uint viewNone = 0;
static const uint viewBlend = 1;
static const uint viewRejected = 2;
static const uint viewClipped = 3;

struct TemporalConstants
{
    // The size of the pictures in pixels.
    int2 size;

    // Where the texture table has the frame's picture and the result of the last frame, and where
    // the table of writable pictures has the one this frame's result goes into.
    uint sceneTexture;
    uint historyTexture;
    uint outputTexture;

    // For resolving. Where the texture table has how far every pixel moved and the scene's depth, and
    // the depth this pass kept of the last frame. Where the writable table has the picture the depth
    // of this frame is kept in, and the one a view of the pass's working is drawn into.
    uint motionTexture;
    uint depthTexture;
    uint previousDepthTexture;
    uint depthOutput;
    uint debugOutput;

    // A sampler that blends between pixels and repeats nothing.
    uint linearSampler;

    uint mode;
    uint view;

    // Zero in a frame that has no result of the last frame to go on from.
    uint hasHistory;

    // Accumulating: how much the frame counts in the result. Resolving: the least it counts.
    float currentWeight;

    // Resolving. How many standard deviations around the mean of the pixel's neighbours the history
    // may lie, or zero to allow all the colours the neighbours span. How sharply the history is read
    // between pixels, 0.5 being the Catmull-Rom curve. And by which share the depth of the history may
    // differ before it counts as another surface.
    float boxGamma;
    float sharpness;
    float depthTolerance;

    // Resolving: how much the frame counts where the history is read halfway between pixels.
    float betweenPixelsShare;
};
[vk::push_constant] ConstantBuffer<TemporalConstants> constants;

// Brightness, and colour as two differences. The colours around a pixel differ mostly in brightness,
// so a box around them is much smaller along these axes than along red, green and blue.
float3 toYCoCg(float3 colour)
{
    return float3(0.25 * colour.r + 0.5 * colour.g + 0.25 * colour.b, 0.5 * colour.r - 0.5 * colour.b, -0.25 * colour.r + 0.5 * colour.g - 0.25 * colour.b);
}

float3 fromYCoCg(float3 colour)
{
    float rest = colour.x - colour.z;
    return float3(rest + colour.y, colour.x + colour.z, rest - colour.y);
}

int2 inPicture(int2 pixel)
{
    return clamp(pixel, int2(0, 0), constants.size - 1);
}

// The history between its pixels, in xyz, and how many frames it is made of, in w.
//
// A surface that moves by a part of a pixel has its history between pixels, and the plain blend of the
// four around it is a little blurred, every frame again, until the picture is soft. The Catmull-Rom
// curve keeps it sharp. It reads four by four pixels: here as five blended reads, without the corners,
// which count for next to nothing (Jimenez, 2016).
float4 historyAt(float2 uv)
{
    Texture2D history = textures[constants.historyTexture];
    SamplerState linear = samplers[constants.linearSampler];
    float2 size = float2(constants.size);
    float2 texel = 1.0 / size;

    float2 position = uv * size;
    float2 centre = floor(position - 0.5) + 0.5;
    float2 f = position - centre;
    float2 f2 = f * f;
    float2 f3 = f * f2;

    float c = constants.sharpness;
    float2 w0 = -c * f3 + 2.0 * c * f2 - c * f;
    float2 w1 = (2.0 - c) * f3 - (3.0 - c) * f2 + 1.0;
    float2 w2 = -(2.0 - c) * f3 + (3.0 - 2.0 * c) * f2 + c * f;
    float2 w3 = c * f3 - c * f2;

    float2 w12 = w1 + w2;
    float2 uv12 = texel * (centre + w2 / w12);
    float2 uv0 = texel * (centre - 1.0);
    float2 uv3 = texel * (centre + 2.0);

    float4 middle = history.SampleLevel(linear, uv12, 0.0);
    float4 sum = float4(middle.rgb, 1.0) * (w12.x * w12.y)
        + float4(history.SampleLevel(linear, float2(uv12.x, uv0.y), 0.0).rgb, 1.0) * (w12.x * w0.y)
        + float4(history.SampleLevel(linear, float2(uv0.x, uv12.y), 0.0).rgb, 1.0) * (w0.x * w12.y)
        + float4(history.SampleLevel(linear, float2(uv3.x, uv12.y), 0.0).rgb, 1.0) * (w3.x * w12.y)
        + float4(history.SampleLevel(linear, float2(uv12.x, uv3.y), 0.0).rgb, 1.0) * (w12.x * w3.y);

    // The curve can overshoot next to a bright pixel, below black too.
    return float4(max(sum.rgb / sum.a, 0.0), middle.a);
}

void accumulate(int2 pixel)
{
    float3 current = textures[constants.sceneTexture].Load(int3(pixel, 0)).rgb;
    float3 history = textures[constants.historyTexture].Load(int3(pixel, 0)).rgb;
    writableTextures[constants.outputTexture][pixel] = float4(lerp(history, current, constants.currentWeight), 1.0);
}

void resolve(int2 pixel)
{
    Texture2D scenePicture = textures[constants.sceneTexture];
    Texture2D depthPicture = textures[constants.depthTexture];

    // The colours around the pixel, which say what the pixel can look like in this frame, and the
    // nearest thing around it. Depth is 1 at the camera and falls towards 0 with distance.
    float3 current = float3(0.0, 0.0, 0.0);
    float3 lowest = float3(1e30, 1e30, 1e30);
    float3 highest = float3(-1e30, -1e30, -1e30);
    float3 lowestOfCross = lowest;
    float3 highestOfCross = highest;
    float3 sum = float3(0.0, 0.0, 0.0);
    float3 sumOfSquares = float3(0.0, 0.0, 0.0);
    float nearestDepth = -1.0;
    float farthestDepth = 2.0;
    float ownDepth = 0.0;
    int2 nearestPixel = pixel;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            int2 neighbour = inPicture(pixel + int2(x, y));
            float3 colour = toYCoCg(scenePicture.Load(int3(neighbour, 0)).rgb);
            lowest = min(lowest, colour);
            highest = max(highest, colour);
            sum += colour;
            sumOfSquares += colour * colour;
            if (x == 0 || y == 0)
            {
                lowestOfCross = min(lowestOfCross, colour);
                highestOfCross = max(highestOfCross, colour);
            }
            float depth = depthPicture.Load(int3(neighbour, 0)).r;
            farthestDepth = min(farthestDepth, depth);
            if (x == 0 && y == 0)
            {
                current = colour;
                ownDepth = depth;
            }
            if (depth > nearestDepth)
            {
                nearestDepth = depth;
                nearestPixel = neighbour;
            }
        }
    }

    // The box the history has to lie in. The full square of neighbours alone gives it corners that
    // show as blocks, so it is rounded off with the box of the cross (Karis, 2014).
    float3 boxLowest = 0.5 * (lowest + lowestOfCross);
    float3 boxHighest = 0.5 * (highest + highestOfCross);
    if (constants.boxGamma > 0.0)
    {
        // Narrower still: around the mean, as wide as the neighbours scatter (Salvi, 2016).
        float3 mean = sum / 9.0;
        float3 deviation = sqrt(max(sumOfSquares / 9.0 - mean * mean, 0.0));
        boxLowest = max(boxLowest, mean - constants.boxGamma * deviation);
        boxHighest = min(boxHighest, mean + constants.boxGamma * deviation);
    }

    // Where the pixel's surface was a frame ago. An edge is smooth in the history but hard in the
    // motion, so a pixel moves with the nearest thing around it: the edge of what is in front then
    // moves as a whole.
    float2 size = float2(constants.size);
    float2 uv = (float2(pixel) + 0.5) / size;
    float2 motion = textures[constants.motionTexture].Load(int3(nearestPixel, 0)).rg;
    float2 historyUv = uv - motion;
    bool isOnScreen = historyUv.x > 0.0 && historyUv.y > 0.0 && historyUv.x < 1.0 && historyUv.y < 1.0;

    // And whether the history shows that surface. If something else was there, nearer or farther, it
    // is not this pixel's past. An edge lands on another pixel from frame to frame, by the jitter
    // alone, so single depths cannot be compared: the depths around the pixel now and around its place
    // then have to have something in common. On an edge they always do, and the box of colours decides.
    // Where a thing has moved off what is behind it, they do not.
    Texture2D previousDepthPicture = textures[constants.previousDepthTexture];
    int2 previousPixel = int2(historyUv * size);
    float nearestBefore = -1.0;
    float farthestBefore = 2.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float depth = previousDepthPicture.Load(int3(inPicture(previousPixel + int2(x, y)), 0)).r;
            nearestBefore = max(nearestBefore, depth);
            farthestBefore = min(farthestBefore, depth);
        }
    }
    float allowed = 1.0 + constants.depthTolerance;
    bool isSameSurface = nearestBefore * allowed >= farthestDepth && nearestDepth * allowed >= farthestBefore;
    bool hasHistory = constants.hasHistory != 0 && isOnScreen && isSameSurface;

    float4 history = hasHistory ? historyAt(historyUv) : float4(0.0, 0.0, 0.0, 0.0);
    float3 past = toYCoCg(history.rgb);

    // The history is pulled into the box along the line to the box's middle, which keeps its hue
    // better than cutting each number off on its own.
    float3 middle = 0.5 * (boxHighest + boxLowest);
    float3 reach = 0.5 * (boxHighest - boxLowest) + 0.00001;
    float3 fromMiddle = past - middle;
    float3 share = abs(fromMiddle / reach);
    float farthest = max(share.x, max(share.y, share.z));
    float3 kept = farthest > 1.0 ? middle + fromMiddle / farthest : past;

    // Every frame so far counts the same, until the frame counts as little as it ever should.
    // Without a history the frame is all there is.
    //
    // A history that is read between pixels comes out a little blurred, and the blur adds up from
    // frame to frame. The further between pixels it is read, the more the frame counts, which keeps
    // what moves sharp at the price of rougher edges while it moves (Yang and others, 2009).
    float2 betweenPixels = abs(frac(historyUv * size) - 0.5) * 2.0;
    float leastShare = lerp(constants.currentWeight, max(constants.currentWeight, constants.betweenPixelsShare), max(betweenPixels.x, betweenPixels.y));
    float frames = hasHistory ? history.a : 0.0;
    float currentShare = hasHistory ? max(1.0 / (frames + 1.0), leastShare) : 1.0;

    // Mixed as the eye will see them: a very bright sample counts less, or a single one would
    // flicker through the average of many (Karis, 2013).
    float exposure = scene.exposure[0].exposure;
    float currentWeight = currentShare / (1.0 + current.x * exposure);
    float pastWeight = (1.0 - currentShare) / (1.0 + kept.x * exposure);
    float3 result = (current * currentWeight + kept * pastWeight) / (currentWeight + pastWeight);

    float mostFrames = 1.0 / max(constants.currentWeight, 0.001);
    writableTextures[constants.outputTexture][pixel] = float4(max(fromYCoCg(result), 0.0), min(frames + 1.0, mostFrames));
    writableTextures[constants.depthOutput][pixel] = float4(ownDepth, 0.0, 0.0, 0.0);

    if (constants.view == viewNone)
        return;

    // The pass's working, as colours that go to the screen as they are.
    float3 shown = float3(0.0, 0.0, 0.0);
    if (constants.view == viewBlend)
    {
        // How much the frame counts: black for hardly at all, white for all there is.
        shown = float3(currentShare, currentShare, currentShare);
    }
    else if (constants.view == viewRejected)
    {
        // Where there is no history: blue where it lies off the screen, red where it showed another surface.
        if (!isOnScreen)
            shown = float3(0.0, 0.2, 1.0);
        else if (!isSameSurface)
            shown = float3(1.0, 0.1, 0.0);
        else
            shown = float3(0.1, 0.1, 0.1);
    }
    else if (constants.view == viewClipped)
    {
        // How far the history was pulled to fit the box, as brightness on screen: white at a tenth of white.
        float pulled = hasHistory ? length(past - kept) * exposure * 10.0 : 0.0;
        shown = float3(pulled, pulled, pulled);
    }
    writableTextures[constants.debugOutput][pixel] = float4(saturate(shown), 1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    // The last groups reach past the picture's edges.
    int2 pixel = int2(thread.xy);
    if (pixel.x >= constants.size.x || pixel.y >= constants.size.y)
        return;

    if (constants.mode == modeResolve)
        resolve(pixel);
    else
        accumulate(pixel);
}
