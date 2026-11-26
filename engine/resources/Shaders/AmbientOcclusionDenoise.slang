// The last step of ambient occlusion, see AmbientOcclusionPass: every pixel is mixed with the eight
// around it, as far as they lie on the same surface. Several rounds of it reach farther.
//
// The occlusion of a pixel comes from a few lines through it, picked by chance, and is grainy. The
// frames average that away, and this step takes the coarsest of it out before, so that less is left
// to show while the picture moves. It never mixes across an edge, where the neighbour is another
// surface with an occlusion of its own. After XeGTAO (Intel, 2021, MIT licence).

import Textures;

struct DenoiseConstants
{
    // The size of the pictures in pixels.
    int2 size;

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
};
[vk::push_constant] ConstantBuffer<DenoiseConstants> constants;

// Has to match AmbientOcclusion.slang.
static const float storedRange = 1.5;

// A neighbour across a corner counts for less than one across a side.
static const float weightAcrossCorner = 0.85 * 0.5;

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
    writableTextures[constants.outputTexture][pixel] = constants.isLast != 0
        ? float4(saturate(smoothed * storedRange), 0.0, 0.0, 0.0)
        : float4(smoothed, centre.y, 0.0, 0.0);
}
