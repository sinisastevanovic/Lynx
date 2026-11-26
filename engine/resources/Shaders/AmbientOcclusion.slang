// Ambient occlusion: how much of the sky's light reaches every pixel past what stands around it.
// A corner, a crease, the ground under a thing are darker than open ground, because part of the sky
// is hidden from them. See AmbientOcclusionPass.
//
// Worked out from the depth alone. For a pixel, the picture is cut along a few lines through it, each
// in another direction. Along each line and to both sides, the shader looks for the horizon: the
// steepest angle at which something near stands above the pixel's surface. The light that arrives
// between the two horizons, weighted by how squarely it hits the surface, is the share of that cut
// that is open. Other pixels and other frames take other lines, and anti-aliasing over time averages them.
//
// "Practical Real-Time Strategies for Accurate Indirect Occlusion" (Jimenez and others, 2016), as
// XeGTAO does it (Intel, 2021, MIT licence): the numbers below are XeGTAO's, which were fitted so
// that the result comes close to occlusion worked out by tracing rays.

import Textures;

struct OcclusionConstants
{
    // The size of the picture in pixels.
    int2 size;

    // Where the texture table has the distances from the camera in five sizes, and the table of
    // writable pictures the picture for the result.
    uint distanceTexture;
    uint outputTexture;

    // From texture coordinates to a place in front of the camera at distance 1:
    // place = (uv * viewScale + viewOffset, 1) * distance. x is to the right and y down, as on screen.
    float2 viewScale;
    float2 viewOffset;

    // How far around a place something counts as standing in the sky's way, in the scene's units.
    float radius;

    // How many lines through the pixel, and how many places along each side of a line.
    uint lineCount;
    uint stepCount;

    // Which of 64 patterns the lines and places take in this frame.
    uint noiseFrame;

    // Where the texture table has the scene's depth buffer, and the distance of the camera's near
    // plane, which turns a depth into a distance.
    uint depthTexture;
    float nearPlane;
};
[vk::push_constant] ConstantBuffer<OcclusionConstants> constants;

static const float pi = 3.14159265359;
static const float halfPi = 1.57079632679;

// Have to match AmbientOcclusionDepth.slang.
static const float farthestDistance = 65504.0;
static const float fadeShareOfRadius = 0.615;
static const int smallestSize = 4;

// Has to match AmbientOcclusionDenoise.slang: the result can come out a little above 1, and is kept
// with room for that until it has been smoothed.
static const float storedRange = 1.5;

// The places along a line crowd towards the pixel by this power: what is near hides more.
static const float crowdingOfPlaces = 2.0;

// A place this many pixels along a line is read from the next smaller size, as a power of two.
static const float pixelsPerSize = 3.3;

// The result is raised to this power, which makes up for what a few lines and a short radius miss.
static const float finalPower = 2.2;

// The distances around a pixel are half floats, so its surface is moved towards the camera by a little
// more than their precision, or it would stand in its own light.
static const float towardsCamera = 0.9992;

float distanceAt(int2 pixel, int level)
{
    int2 size = max(constants.size >> level, int2(1, 1));
    return textures[constants.distanceTexture].Load(int3(clamp(pixel >> level, int2(0, 0), size - 1), level)).r;
}

// The distance of a pixel and of those right next to it, from the depth buffer itself: the way a
// surface faces is worked out from differences between them that are too small for half floats,
// which would show as streaks on every wall. The scene's depth is reversed and has no far plane:
// 1 at the near plane, towards 0 far away, and exactly 0 where nothing is.
float exactDistanceAt(int2 pixel)
{
    float depth = textures[constants.depthTexture].Load(int3(clamp(pixel, int2(0, 0), constants.size - 1), 0)).r;
    return depth > 0.0 ? min(constants.nearPlane / depth, farthestDistance) : farthestDistance;
}

float3 placeAt(float2 uv, float howFar)
{
    return float3((uv * constants.viewScale + constants.viewOffset) * howFar, howFar);
}

float2 uvOf(int2 pixel)
{
    return (float2(pixel) + 0.5) / float2(constants.size);
}

// How much the pixel and each of its four neighbours, left, right, above and below, belong to one
// surface: 1 for the same, 0 across an edge. A slope is no edge, so the difference to a neighbour
// also counts as it is after the slope through the pixel is taken out.
float4 sameSurface(float centre, float4 neighbours)
{
    float4 differences = neighbours - centre;
    float slopeAcross = (differences.y - differences.x) * 0.5;
    float slopeDown = (differences.w - differences.z) * 0.5;
    float4 withoutSlope = differences + float4(slopeAcross, -slopeAcross, slopeDown, -slopeDown);
    differences = min(abs(differences), abs(withoutSlope));
    return saturate(1.25 - differences / (centre * 0.011));
}

// The four as one number of eight bits, two bits each. Has to match AmbientOcclusionDenoise.slang.
float asOneNumber(float4 sameSurface)
{
    return dot(round(saturate(sameSurface) * 2.9), float4(64.0, 16.0, 4.0, 1.0) / 255.0);
}

// The way the surface faces, towards the camera, from the pixel's place and its neighbours'. A neighbour
// across an edge is on another surface, so each pair of neighbours counts by how much both belong.
float3 normalFrom(float4 sameSurface, float3 centre, float3 left, float3 right, float3 above, float3 below)
{
    float4 pairs = saturate(float4(sameSurface.x * sameSurface.z, sameSurface.z * sameSurface.y,
        sameSurface.y * sameSurface.w, sameSurface.w * sameSurface.x) + 0.01);
    left = normalize(left - centre);
    right = normalize(right - centre);
    above = normalize(above - centre);
    below = normalize(below - centre);
    return normalize(pairs.x * cross(above, left) + pairs.y * cross(right, above) + pairs.z * cross(below, right) + pairs.w * cross(left, below));
}

// Where a pixel lies along a curve that visits every pixel of a square of 64 by 64 once and never
// jumps (Hilbert's curve). Pixels next to each other on screen are near each other on it.
uint alongCurve(uint2 pixel)
{
    uint index = 0;
    for (uint level = 32; level > 0; level /= 2)
    {
        uint regionX = (pixel.x & level) > 0 ? 1 : 0;
        uint regionY = (pixel.y & level) > 0 ? 1 : 0;
        index += level * level * ((3 * regionX) ^ regionY);
        if (regionY == 0)
        {
            if (regionX == 1)
                pixel = uint2(63, 63) - pixel;
            pixel = pixel.yx;
        }
    }
    return index;
}

// Two numbers from 0 to 1 that differ from pixel to pixel and from frame to frame so that pixels
// next to each other, and frames after each other, between them cover all there is to try evenly:
// the R2 sequence, taken along the curve above (XeGTAO).
float2 noiseOf(uint2 pixel)
{
    uint index = alongCurve(pixel % 64) + 288 * (constants.noiseFrame % 64);
    return frac(0.5 + float(index) * float2(0.75487766624669276005, 0.5698402909980532659114));
}

// The angle of a cosine, off by less than a hundredth of a degree's worth of light.
float angleOf(float cosine)
{
    float x = abs(cosine);
    float angle = (-0.156583 * x + halfPi) * sqrt(1.0 - x);
    return cosine >= 0.0 ? angle : pi - angle;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    // The last groups reach past the picture's edges.
    int2 pixel = int2(thread.xy);
    if (pixel.x >= constants.size.x || pixel.y >= constants.size.y)
        return;

    // The sky is hidden from nothing, and belongs to no surface next to it.
    float howFar = exactDistanceAt(pixel);
    if (howFar >= farthestDistance)
    {
        writableTextures[constants.outputTexture][pixel] = float4(1.0 / storedRange, 0.0, 0.0, 0.0);
        return;
    }

    float left = exactDistanceAt(pixel + int2(-1, 0));
    float right = exactDistanceAt(pixel + int2(1, 0));
    float above = exactDistanceAt(pixel + int2(0, -1));
    float below = exactDistanceAt(pixel + int2(0, 1));
    float4 edges = sameSurface(howFar, float4(left, right, above, below));

    float2 uv = uvOf(pixel);
    float3 normal = normalFrom(edges, placeAt(uv, howFar), placeAt(uvOf(pixel + int2(-1, 0)), left), placeAt(uvOf(pixel + int2(1, 0)), right),
        placeAt(uvOf(pixel + int2(0, -1)), above), placeAt(uvOf(pixel + int2(0, 1)), below));

    howFar *= towardsCamera;
    float3 centre = placeAt(uv, howFar);
    float3 toEye = normalize(-centre);

    // What stands within the radius counts in full up to a part of it, and fades out over the rest:
    // a hard end would show as a line where something comes into reach.
    float radius = constants.radius;
    float fade = fadeShareOfRadius * radius;
    float fadeFrom = radius * (1.0 - fadeShareOfRadius);
    float fadePerDistance = -1.0 / fade;
    float fadeAtZero = fadeFrom / fade + 1.0;

    // The radius in pixels at the pixel's distance.
    float pixelSize = howFar * constants.viewScale.x / float(constants.size.x);
    float radiusInPixels = radius / pixelSize;

    // Far away the radius shrinks to a few pixels, which is too few to tell anything by: the result
    // fades to open there.
    float open = saturate((10.0 - radiusInPixels) / 100.0) * 0.5;

    // A place nearer than this along a line would be the pixel itself.
    float nearestPlace = 1.3 / radiusInPixels;

    float2 noise = noiseOf(uint2(pixel));
    float lineCount = float(constants.lineCount);
    float stepCount = float(constants.stepCount);

    for (uint cut = 0; cut < constants.lineCount; ++cut)
    {
        float turn = (float(cut) + noise.x) / lineCount * pi;
        float2 onScreen = float2(cos(turn), sin(turn));
        float3 direction = float3(onScreen, 0.0);

        // The cut through the pixel along the line holds the direction to the eye. The normal is
        // brought into it, and its angle to the eye within the cut is where the two horizons start:
        // a quarter turn to either side of it, the surface's own plane.
        float3 acrossEye = direction - dot(direction, toEye) * toEye;
        float3 axis = normalize(cross(acrossEye, toEye));
        float3 normalInCut = normal - axis * dot(normal, axis);
        float lengthInCut = length(normalInCut);
        float cosineOfNormal = saturate(dot(normalInCut, toEye) / lengthInCut);
        float angleOfNormal = sign(dot(acrossEye, normalInCut)) * angleOf(cosineOfNormal);

        float2 lowest = float2(cos(angleOfNormal + halfPi), cos(angleOfNormal - halfPi));
        float2 horizons = lowest;

        for (uint place = 0; place < constants.stepCount; ++place)
        {
            // Along the line the places are spread by the golden ratio, so that no two lines of a
            // pixel try the same distances.
            float shift = frac(noise.y + float(cut + place * constants.stepCount) * 0.6180339887498948482);
            float along = pow((float(place) + shift) / stepCount, crowdingOfPlaces) + nearestPlace;

            float2 offset = along * radiusInPixels * onScreen;
            int level = int(clamp(log2(length(offset)) - pixelsPerSize, 0.0, float(smallestSize)) + 0.5);
            int2 wholeOffset = int2(round(offset));

            int2 pixelOnOneSide = pixel + wholeOffset;
            int2 pixelOnOtherSide = pixel - wholeOffset;
            float3 toOneSide = placeAt(uvOf(pixelOnOneSide), distanceAt(pixelOnOneSide, level)) - centre;
            float3 toOtherSide = placeAt(uvOf(pixelOnOtherSide), distanceAt(pixelOnOtherSide, level)) - centre;

            float2 distances = float2(length(toOneSide), length(toOtherSide));
            float2 counts = saturate(distances * fadePerDistance + fadeAtZero);
            float2 cosines = float2(dot(toOneSide / distances.x, toEye), dot(toOtherSide / distances.y, toEye));
            horizons = max(horizons, lerp(lowest, cosines, counts));
        }

        // XeGTAO's correction, fitted against traced rays: a normal that leans out of the cut counts
        // for a little more than its length within it.
        lengthInCut = lerp(lengthInCut, 1.0, 0.05);

        // The light between the two horizons, weighted by the cosine to the normal (Jimenez, 2016).
        float2 angles = float2(angleOf(horizons.x), -angleOf(horizons.y));
        float2 arcs = (cosineOfNormal + 2.0 * angles * sin(angleOfNormal) - cos(2.0 * angles - angleOfNormal)) / 4.0;
        open += lengthInCut * (arcs.x + arcs.y);
    }

    open = max(pow(open / lineCount, finalPower), 0.03);
    writableTextures[constants.outputTexture][pixel] = float4(saturate(open / storedRange), asOneNumber(edges), 0.0, 0.0);
}
