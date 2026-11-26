// Works out how bright the picture is made, from how bright the scene in view is, as a camera's
// automatic exposure does. In two steps, chosen by "step", which the pass runs one after the other:
//   Count: looks at the scene's picture and counts how many places have which brightness, into a
//     histogram of 256 bins.
//   Adapt: finds the brightness in the middle of the histogram and moves the exposure a little towards
//     the one that shows that brightness as a middle grey.
// After "Automatic Exposure Using a Luminance Histogram" (Tardif, 2019). The exposure stays on the GPU
// from frame to frame, in a small buffer that the scene constants point to, so nothing is read back.

import Scene;
import Textures;

static const uint stepCount = 0;
static const uint stepAdapt = 1;

static const uint binCount = 256;

// The brightnesses the histogram tells apart: from 2 to the power of the first number to 2 to the
// power of the second, 22 stops. Darker than that all counts as black, in the first bin.
static const float darkestStop = -12.0;
static const float brightestStop = 10.0;

// The darkest and the brightest tenth of the picture do not count: a dark corner or a bright sky
// should not decide how the rest is shown.
static const float lowestShare = 0.1;
static const float highestShare = 0.9;

// The brightness of a middle grey on the screen.
static const float middleGrey = 0.18;

struct ExposureConstants
{
    uint* histogram;

    // How many places are looked at across and down, and how large the scene's picture is in pixels.
    int2 places;
    int2 pictureSize;

    // Where the texture table has the scene's picture.
    uint sceneTexture;
    uint step;

    // The time since the last frame in seconds.
    float deltaTime;

    // The exposure to set, or less than zero for the automatic one.
    float fixedExposure;

    // The automatic exposure: what it is multiplied by, the range it stays in, and how fast it follows.
    float compensation;
    float lowest;
    float highest;
    float speedTowardsDarker;
    float speedTowardsBrighter;
    uint padding;
};
[vk::push_constant] ConstantBuffer<ExposureConstants> constants;

float brightnessOf(float3 colour)
{
    return dot(colour, float3(0.2126, 0.7152, 0.0722));
}

void count(int2 place)
{
    if (place.x >= constants.places.x || place.y >= constants.places.y)
        return;

    // The pixel in the middle of the patch of the picture this place stands for.
    int2 pixel = min((place * constants.pictureSize + constants.pictureSize / 2) / constants.places, constants.pictureSize - 1);
    float brightness = brightnessOf(textures[constants.sceneTexture].Load(int3(pixel, 0)).rgb);

    uint bin = 0;
    if (brightness > exp2(darkestStop))
    {
        float along = saturate((log2(brightness) - darkestStop) / (brightestStop - darkestStop));
        bin = 1 + uint(along * float(binCount - 2));
    }
    InterlockedAdd(constants.histogram[bin], 1);
}

void adapt()
{
    if (constants.fixedExposure >= 0.0)
    {
        scene.exposure[0].exposure = constants.fixedExposure;
        scene.exposure[0].isAdapted = 0;
        return;
    }

    uint total = 0;
    for (uint bin = 0; bin < binCount; ++bin)
        total += constants.histogram[bin];

    // The stops of the places between the two shares, averaged. A bin that the lower share ends in
    // or the upper one begins in counts with the part of it that lies between them.
    float first = float(total) * lowestShare;
    float last = float(total) * highestShare;
    float before = 0.0;
    float counted = 0.0;
    float stops = 0.0;
    for (uint bin = 0; bin < binCount; ++bin)
    {
        float inBin = float(constants.histogram[bin]);
        float taken = max(min(before + inBin, last) - max(before, first), 0.0);
        before += inBin;

        float stop = bin == 0 ? darkestStop : darkestStop + (float(bin - 1) + 0.5) / float(binCount - 2) * (brightestStop - darkestStop);
        stops += stop * taken;
        counted += taken;
    }

    // Nothing was counted: the pass looked at no picture. The exposure stays.
    if (counted <= 0.0)
        return;

    float wanted = clamp(middleGrey / exp2(stops / counted) * constants.compensation, constants.lowest, constants.highest);

    // The first time there is nothing to come from.
    float current = scene.exposure[0].exposure;
    if (scene.exposure[0].isAdapted == 0 || current <= 0.0)
    {
        scene.exposure[0].exposure = wanted;
        scene.exposure[0].isAdapted = 1;
        return;
    }

    // Followed in stops, so that twice as bright and half as bright take the same time. The share
    // of the way that is covered in a frame is such that the frame rate does not matter.
    float speed = wanted < current ? constants.speedTowardsDarker : constants.speedTowardsBrighter;
    float share = 1.0 - exp(-constants.deltaTime * speed);
    scene.exposure[0].exposure = exp2(lerp(log2(current), log2(wanted), share));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    if (constants.step == stepCount)
        count(int2(thread.xy));
    else if (thread.x == 0 && thread.y == 0)
        adapt();
}
