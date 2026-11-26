// The last step of ambient occlusion, see AmbientOcclusionPass: every pixel is mixed with the eight
// around it, as far as they lie on the same surface. Several rounds of it reach farther.
//
// The occlusion of a pixel comes from a few lines through it, picked by chance, and is grainy. This
// step takes the coarsest of it out. It never mixes across an edge, where the neighbour is another
// surface with an occlusion of its own. After XeGTAO (Intel, 2021, MIT licence).
//
// The last round then mixes the result with that of the frames before, the history, fetched from
// where the pixel's surface was a frame ago. Smoothing only takes out the grain of single pixels.
// What is left differs from frame to frame, and while the picture moves, anti-aliasing over time
// trusts its own history too little to average it away: it has detail to keep sharp. Occlusion has
// none, so it can take a long history of its own ("Practical Real-Time Strategies for Accurate
// Indirect Occlusion", Jimenez and others, 2016).

import Textures;

struct DenoiseConstants
{
    // From a place on screen in this frame to where the same place of the world was in the last
    // frame, both without the camera's jitter: in goes (x and y from -1 to 1, depth, 1).
    float4x4 toLastFrame;

    // The size of the pictures in pixels, and how far the frame's jitter shifts the picture, in
    // texture coordinates.
    int2 size;
    float2 jitter;

    // Where the texture table has the occlusion as it was worked out, with how much every pixel and
    // its four neighbours belong to one surface next to it, and the table of writable pictures the
    // picture for the result.
    uint occlusionTexture;
    uint outputTexture;

    // How much the pixel itself counts next to its neighbours, each of which counts for up to 1.
    // A large number leaves the pixel as it is.
    float weightOfCentre;

    // Not zero for the last round, whose result is what the frame uses. The rounds before it hand
    // on what they read, smoothed, for the next round to read.
    uint isLast;

    // For the last round. Where the texture table has the scene's depth buffer, and the distance of
    // the camera's near plane, which turns a depth into a distance.
    uint depthTexture;
    float nearPlane;

    // Where the texture table has the result of the last frame, as this shader wrote it, and zero
    // if there is none to go on from.
    uint historyTexture;
    uint hasHistory;

    // The least the frame counts against the history.
    float leastShare;

    // A sampler that blends between pixels and repeats nothing.
    uint linearSampler;

    // How many pixels the occlusion looks around a place that is one unit from the camera.
    float reachInPixels;

    // Zero if no history is kept: the result is then the frame's alone, and nothing is kept with it.
    uint keepsHistory;
};
[vk::push_constant] ConstantBuffer<DenoiseConstants> constants;

// Have to match AmbientOcclusion.slang.
static const float storedRange = 1.5;
static const float farthestDistance = 65504.0;

// A neighbour across a corner counts for less than one across a side.
static const float weightAcrossCorner = 0.85 * 0.5;

// The history counts as the same surface while its distance from the camera is within this share
// of where the pixel's surface was, plus what the surface's own slope makes of a pixel and a half,
// and fades out over as much again. A fixed share alone is either too narrow for a floor seen at
// a flat angle, whose distance changes by hundredths from one pixel to the next, or so wide that a
// wall takes over the history of a plant that hung in front of it: that showed as the plant's
// shape trailing over the wall.
static const float sameDistance = 0.01;
static const float pixelsOfSlope = 1.5;

// No slope counts as steeper than this share of the distance a pixel: a surface seen flatter
// than that covers next to nothing of the screen.
static const float steepestSlope = 0.02;

// The history never counts as more frames than this, however long it has been kept.
static const float mostFramesCounted = 64.0;

// What AmbientOcclusion.slang wrote for a pixel: its occlusion, and its edges as one number.
float2 writtenAt(int2 pixel)
{
    return textures[constants.occlusionTexture].Load(int3(clamp(pixel, int2(0, 0), constants.size - 1), 0)).rg;
}

// How much a pixel and its neighbours to the left, to the right, above and below belong to one
// surface, from the one number AmbientOcclusion.slang packed them into.
float4 sameSurface(float asOneNumber)
{
    uint bits = uint(asOneNumber * 255.5);
    return saturate(float4((bits >> 6) & 3, (bits >> 4) & 3, (bits >> 2) & 3, bits & 3) / 3.0);
}

// The scene's depth is reversed and has no far plane: 1 at the near plane, towards 0 far away, and
// exactly 0 where nothing is.
float distanceOf(float depth)
{
    return depth > 0.0 ? min(constants.nearPlane / depth, farthestDistance) : farthestDistance;
}

float distanceAt(int2 pixel)
{
    return distanceOf(textures[constants.depthTexture].Load(int3(clamp(pixel, int2(0, 0), constants.size - 1), 0)).r);
}

// The occlusion of the last frame between its pixels, read with the Catmull-Rom curve.
//
// A surface that moves by a part of a pixel has its history between pixels, and the plain blend of
// the four around it is a little blurred, every frame again: after a quarter of a second the
// occlusion of a plant's leaves had spread a few pixels onto the wall behind it. The curve keeps
// it sharp. It reads four by four pixels, here as five blended reads without the corners
// (Jimenez, 2016), as anti-aliasing over time reads its history. It knows nothing of surfaces, so
// it is only used where everything around the place is one.
float sharpHistoryAt(float2 uv)
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

    float2 w0 = -0.5 * f3 + f2 - 0.5 * f;
    float2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    float2 w2 = -1.5 * f3 + 2.0 * f2 + 0.5 * f;
    float2 w3 = 0.5 * f3 - 0.5 * f2;

    float2 w12 = w1 + w2;
    float2 uv12 = texel * (centre + w2 / w12);
    float2 uv0 = texel * (centre - 1.0);
    float2 uv3 = texel * (centre + 2.0);

    float2 sum = float2(history.SampleLevel(linear, uv12, 0.0).r, 1.0) * (w12.x * w12.y)
        + float2(history.SampleLevel(linear, float2(uv12.x, uv0.y), 0.0).r, 1.0) * (w12.x * w0.y)
        + float2(history.SampleLevel(linear, float2(uv0.x, uv12.y), 0.0).r, 1.0) * (w0.x * w12.y)
        + float2(history.SampleLevel(linear, float2(uv3.x, uv12.y), 0.0).r, 1.0) * (w3.x * w12.y)
        + float2(history.SampleLevel(linear, float2(uv12.x, uv3.y), 0.0).r, 1.0) * (w12.x * w3.y);
    return sum.x / sum.y;
}

// How much the distance of a surface changes over a pixel one way across the screen, from the
// distances of a pixel and of the two next to it that way. The smaller of the two steps, because
// at an edge one of them is a step onto another surface. And none where the pixel is nearer or
// farther than both: a leaf in front of a wall has no slope to go by, and would otherwise be
// given the whole way to the wall.
float slopeBetween(float before, float at, float after)
{
    float stepIn = at - before;
    float stepOut = after - at;
    return stepIn * stepOut > 0.0 ? min(abs(stepIn), abs(stepOut)) : 0.0;
}

// The result of the frames before for the pixel's surface, in x, and how many frames it is made of,
// in y: zero where there is none, because the surface was off screen or hidden then.
// howFar is the pixel's distance from the camera, and slope how much that changes over a pixel.
//
// The scene's motion is not drawn yet when this runs, so the place is worked out from the camera
// alone. Something that moves by itself is looked up where it would be if it stood still. Where
// that shows another surface, the distance tells and the history is dropped. Where it shows the
// thing itself a little further along, its occlusion lags by that much, which is not seen: it
// differs slowly across a surface.
//
// The four pixels of the history around the place are mixed as a texture read between pixels
// mixes them, but each only as far as it shows a surface at the distance the pixel's was.
// Where all four do, the history is read with the Catmull-Rom curve instead, see sharpHistoryAt.
// The jitter is left out of the place, so a view at rest reads exactly its own pixel.
float2 historyOf(int2 pixel, float depth, float howFar, float slope)
{
    if (constants.hasHistory == 0 || depth <= 0.0)
        return float2(1.0, 0.0);

    float2 uv = (float2(pixel) + 0.5) / float2(constants.size);
    float2 onScreen = (uv - constants.jitter) * 2.0 - 1.0;
    float4 before = mul(constants.toLastFrame, float4(onScreen, depth, 1.0));
    if (before.w <= 0.0)
        return float2(1.0, 0.0);

    // In clip space w is the distance from the camera, here as a share of this frame's.
    float howFarBefore = before.w * howFar;
    float allowed = sameDistance * howFarBefore + pixelsOfSlope * slope;
    float2 place = (uv + (before.xy / before.w - onScreen) * 0.5) * float2(constants.size) - 0.5;
    int2 first = int2(floor(place));
    float2 between = place - float2(first);

    float2 sum = float2(0.0, 0.0);
    float weights = 0.0;
    float2 span = float2(1.0, 0.0);
    for (int index = 0; index < 4; ++index)
    {
        int2 corner = int2(index & 1, index >> 1);
        int2 at = first + corner;
        if (at.x < 0 || at.y < 0 || at.x >= constants.size.x || at.y >= constants.size.y)
            continue;

        float3 kept = textures[constants.historyTexture].Load(int3(at, 0)).rgb;
        float2 share = lerp(1.0 - between, between, float2(corner));
        float weight = share.x * share.y * saturate(2.0 - abs(kept.g - howFarBefore) / allowed);
        sum += kept.rb * weight;
        weights += weight;
        span = float2(min(span.x, kept.r), max(span.y, kept.r));
    }

    // Too little of the place is left to go by.
    if (weights < 0.05)
        return float2(1.0, 0.0);

    float2 history = sum / weights;

    // The occlusion of a place near the picture's edge was worked out without what lies off screen
    // around it, and is only a guess. Kept as history, the guess would be carried into the picture
    // as the camera turns, and stay for as long as the history lasts: it showed as a band a hundred
    // pixels wide. So such a history counts as fewer frames than it is made of, by how much of what
    // the occlusion looked at was on screen. Most of that lies within half its reach.
    float2 toEdge = min(place, float2(constants.size) - 1.0 - place);
    history.y *= saturate(min(toEdge.x, toEdge.y) / (0.5 * constants.reachInPixels / howFarBefore));

    if (weights > 0.99)
        history.x = clamp(sharpHistoryAt((place + 0.5) / float2(constants.size)), span.x, span.y);
    return history;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    // The last groups reach past the picture's edges.
    int2 pixel = int2(thread.xy);
    if (pixel.x >= constants.size.x || pixel.y >= constants.size.y)
        return;

    float2 centre = writtenAt(pixel);
    float2 left = writtenAt(pixel + int2(-1, 0));
    float2 right = writtenAt(pixel + int2(1, 0));
    float2 above = writtenAt(pixel + int2(0, -1));
    float2 below = writtenAt(pixel + int2(0, 1));

    float4 ofCentre = sameSurface(centre.y);
    float4 ofLeft = sameSurface(left.y);
    float4 ofRight = sameSurface(right.y);
    float4 ofAbove = sameSurface(above.y);
    float4 ofBelow = sameSurface(below.y);

    // Two pixels belong together only if each says so of the other.
    ofCentre *= float4(ofLeft.y, ofRight.x, ofAbove.w, ofBelow.z);

    // A pixel with edges on nearly every side would keep all of its grain. It is let mix a little
    // across them: a thin thing that is a little off is better than one that sparkles.
    float edginess = saturate(4.0 - 2.5 - dot(ofCentre, float4(1.0, 1.0, 1.0, 1.0))) / (4.0 - 2.5) * 0.5;
    ofCentre = saturate(ofCentre + edginess);

    // A neighbour across a corner is reached by two ways around it, and counts by both.
    float aboveLeft = weightAcrossCorner * (ofCentre.x * ofLeft.z + ofCentre.z * ofAbove.x);
    float aboveRight = weightAcrossCorner * (ofCentre.z * ofAbove.y + ofCentre.y * ofRight.z);
    float belowLeft = weightAcrossCorner * (ofCentre.w * ofBelow.x + ofCentre.x * ofLeft.w);
    float belowRight = weightAcrossCorner * (ofCentre.y * ofRight.w + ofCentre.w * ofBelow.y);

    float sum = centre.x * constants.weightOfCentre
        + left.x * ofCentre.x + right.x * ofCentre.y + above.x * ofCentre.z + below.x * ofCentre.w
        + writtenAt(pixel + int2(-1, -1)).x * aboveLeft + writtenAt(pixel + int2(1, -1)).x * aboveRight
        + writtenAt(pixel + int2(-1, 1)).x * belowLeft + writtenAt(pixel + int2(1, 1)).x * belowRight;
    float weights = constants.weightOfCentre + dot(ofCentre, float4(1.0, 1.0, 1.0, 1.0)) + aboveLeft + aboveRight + belowLeft + belowRight;
    float smoothed = sum / weights;

    if (constants.isLast == 0)
    {
        writableTextures[constants.outputTexture][pixel] = float4(smoothed, centre.y, 0.0, 0.0);
        return;
    }

    float open = saturate(smoothed * storedRange);
    if (constants.keepsHistory == 0)
    {
        writableTextures[constants.outputTexture][pixel] = float4(open, 0.0, 0.0, 0.0);
        return;
    }

    float depth = textures[constants.depthTexture].Load(int3(pixel, 0)).r;
    float howFar = distanceOf(depth);

    float slope = min(slopeBetween(distanceAt(pixel + int2(-1, 0)), howFar, distanceAt(pixel + int2(1, 0)))
        + slopeBetween(distanceAt(pixel + int2(0, -1)), howFar, distanceAt(pixel + int2(0, 1))), steepestSlope * howFar);

    // A surface that has just come into view starts from its first frame and takes every further
    // one as an equal part, until the frames are so many that each counts the least it may.
    float2 history = historyOf(pixel, depth, howFar, slope);
    float share = max(1.0 / (history.y + 1.0), constants.leastShare);
    open = lerp(history.x, open, share);

    // Next to the occlusion, what the next frame needs to go on from it: the distance of the pixel
    // and the number of frames the result is made of.
    writableTextures[constants.outputTexture][pixel] = float4(open, howFar, min(history.y + 1.0, mostFramesCounted), 0.0);
}
