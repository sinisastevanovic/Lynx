// The air above the ground and what it does to sunlight: the colour of the sky in every direction and
// the colour of the sun when it reaches the ground. After "A Scalable and Production Ready Sky and
// Atmosphere Rendering Technique" (Hillaire, 2020).
//
// Light is followed through the air in steps, which is far too slow to do per pixel of the screen.
// So the answers are kept in three small pictures, the tables, and a pixel reads them:
//   Transmittance: how much light gets from a point to the top of the air, by height and direction.
//     Made once for an atmosphere.
//   Multiple scattering: light that reaches a point after bouncing in the air more than once, by height
//     and height of the sun. Made once for an atmosphere, from the first table.
//   Sky view: the sky as the camera sees it in every direction. Made every frame, from the other two.
// AtmosphereTables.slang fills them, Sky.slang draws the sky from the third.
//
// Everything here is in kilometres, with the middle of the planet at the origin and the camera straight
// above it on the y axis. Light is measured for a sun of brightness one and multiplied by the real one
// at the very end, so the first two tables do not depend on the sun at all.
module Atmosphere;

import Scene;
import Textures;

static const float pi = 3.14159265359;

// The sizes of the tables in pixels. Have to match Atmosphere.h.
public static const float2 transmittanceTableSize = float2(256.0, 64.0);
public static const float2 multiScatteringTableSize = float2(32.0, 32.0);
public static const float2 skyViewTableSize = float2(192.0, 108.0);

// Something on the ground sits a little above the planet's sphere, or a ray from it towards the sun
// could count as starting below the ground.
static const float aboveGround = 0.01;

float groundRadius() { return scene.atmosphere.radii.x; }
float topRadius() { return scene.atmosphere.radii.y; }

// The direction towards the sun.
public float3 towardsSun()
{
    return -normalize(scene.lightDirection.xyz);
}

// How far along a ray the sphere around the origin is met first, or -1 if it is not met.
// From inside the sphere that is the way out.
public float distanceToSphere(float3 origin, float3 direction, float radius)
{
    float halfB = dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = halfB * halfB - c;
    if (discriminant < 0.0)
        return -1.0;

    float root = sqrt(discriminant);
    float nearer = -halfB - root;
    float farther = -halfB + root;
    if (nearer >= 0.0)
        return nearer;
    return farther >= 0.0 ? farther : -1.0;
}

// A table is read between the middles of its pixels. These two stretch a value from 0 to 1 over that
// range and back, so that 0 and 1 land exactly on the first and the last pixel.
float2 toPixelMiddles(float2 unit, float2 size)
{
    return (unit + 0.5 / size) * (size / (size + 1.0));
}

float2 fromPixelMiddles(float2 uv, float2 size)
{
    return (uv - 0.5 / size) * (size / (size - 1.0));
}

// What the air is made of at a place, per kilometre of the way through it.
struct Medium
{
    // Light thrown in other directions by the air's molecules, which is what makes the sky blue,
    // and by dust and droplets, which is the white glare around the sun.
    float3 scatteringByAir;
    float3 scatteringByDust;
    float3 scattering;

    // Light lost on the way: all that is scattered, and what dust and ozone swallow.
    float3 extinction;
};

Medium mediumAt(float3 position)
{
    float height = max(length(position) - groundRadius(), 0.0);

    // Air and dust thin out with height. Ozone is a layer around 25 km.
    float air = exp(-height * scene.atmosphere.rayleigh.w);
    float dust = exp(-height * scene.atmosphere.mie.z);
    float ozone = saturate(1.0 - abs(height - 25.0) / 15.0);

    Medium medium;
    medium.scatteringByAir = scene.atmosphere.rayleigh.xyz * air;
    medium.scatteringByDust = scene.atmosphere.mie.xxx * dust;
    medium.scattering = medium.scatteringByAir + medium.scatteringByDust;
    medium.extinction = medium.scattering + scene.atmosphere.mie.yyy * dust + scene.atmosphere.ozone.xyz * ozone;
    return medium;
}

// How air spreads the light it scatters over directions, by the cosine of the angle to the light's way.
float phaseOfAir(float cosine)
{
    return 3.0 / (16.0 * pi) * (1.0 + cosine * cosine);
}

// The same for dust, which throws most of the light on forwards (Cornette and Shanks).
float phaseOfDust(float cosine)
{
    float g = scene.atmosphere.mie.w;
    float k = 3.0 / (8.0 * pi) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cosine * cosine) / pow(1.0 + g * g - 2.0 * g * cosine, 1.5);
}

static const float phaseOfNothing = 1.0 / (4.0 * pi);

// The transmittance table is laid out by a height and a direction (Bruneton's layout): along x how far
// it is to the top of the air in that direction, between the shortest and the longest way there is at
// that height, and along y the height, as the distance to the horizon. That spends the pixels where
// the values change, near the horizon.
// radius is the distance from the planet's middle, upwards the cosine of the angle to straight up.
float2 transmittanceUv(float radius, float upwards)
{
    float ground = groundRadius();
    float top = topRadius();
    float horizonFromTop = sqrt(max(top * top - ground * ground, 0.0));
    float horizon = sqrt(max(radius * radius - ground * ground, 0.0));

    float discriminant = radius * radius * (upwards * upwards - 1.0) + top * top;
    float toTop = max(-radius * upwards + sqrt(max(discriminant, 0.0)), 0.0);
    float shortest = top - radius;
    float longest = horizon + horizonFromTop;
    return float2((toTop - shortest) / (longest - shortest), horizon / horizonFromTop);
}

public void transmittanceParameters(float2 uv, out float radius, out float upwards)
{
    float ground = groundRadius();
    float top = topRadius();
    float horizonFromTop = sqrt(top * top - ground * ground);
    float horizon = horizonFromTop * uv.y;
    radius = sqrt(horizon * horizon + ground * ground);

    float shortest = top - radius;
    float longest = horizon + horizonFromTop;
    float toTop = shortest + uv.x * (longest - shortest);
    upwards = toTop == 0.0 ? 1.0 : (horizonFromTop * horizonFromTop - horizon * horizon - toTop * toTop) / (2.0 * radius * toTop);
    upwards = clamp(upwards, -1.0, 1.0);
}

// How much light gets from a place to the top of the air in a direction, read from the table.
public float3 transmittanceToTop(float radius, float upwards)
{
    float2 uv = transmittanceUv(radius, upwards);
    return textures[scene.atmosphere.tables.x].SampleLevel(samplers[scene.atmosphere.tables.w], uv, 0.0).rgb;
}

// The same, worked out step by step: this is what fills the table.
public float3 transmittanceToTopBySteps(float radius, float upwards)
{
    static const int stepCount = 40;

    float3 position = float3(0.0, radius, 0.0);
    float3 direction = float3(sqrt(max(1.0 - upwards * upwards, 0.0)), upwards, 0.0);
    float distance = distanceToSphere(position, direction, topRadius());
    if (distance <= 0.0)
        return float3(1.0, 1.0, 1.0);

    float stepLength = distance / float(stepCount);
    float3 depth = float3(0.0, 0.0, 0.0);
    for (int step = 0; step < stepCount; ++step)
        depth += mediumAt(position + direction * ((float(step) + 0.5) * stepLength)).extinction * stepLength;
    return exp(-depth);
}

// Light that reaches a place after more than one bounce in the air, for a sun of brightness one,
// read from the table. sunUpwards is the cosine of the angle between straight up and the sun.
float3 multipleScattering(float radius, float sunUpwards)
{
    float2 unit = saturate(float2(sunUpwards * 0.5 + 0.5, (radius - groundRadius()) / (topRadius() - groundRadius())));
    float2 uv = toPixelMiddles(unit, multiScatteringTableSize);
    return textures[scene.atmosphere.tables.y].SampleLevel(samplers[scene.atmosphere.tables.w], uv, 0.0).rgb;
}

// What following a ray through the air gives.
public struct Scattered
{
    // The light the air sends back along the ray, for a sun of brightness one.
    public float3 light;

    // The same if every place were lit with brightness one from everywhere: the share of light that
    // is scattered once more. Only the multiple scattering table asks for it.
    public float3 scatteredAgain;
};

// Follows a ray from a place through the air to where it leaves it or meets the ground.
//   sunDirection: towards the sun.
//   usesPhase: false spreads scattered light evenly over all directions, as light does after many bounces.
//   lightsGround: true adds the sunlight the ground throws back where the ray ends on it.
//   usesMultipleScattering: true adds the light of further bounces from its table.
public Scattered scatteredAlong(float3 origin, float3 direction, float3 sunDirection, int stepCount, bool usesPhase, bool lightsGround,
    bool usesMultipleScattering)
{
    Scattered result;
    result.light = float3(0.0, 0.0, 0.0);
    result.scatteredAgain = float3(0.0, 0.0, 0.0);

    float toGround = distanceToSphere(origin, direction, groundRadius());
    float toTop = distanceToSphere(origin, direction, topRadius());
    float distance = toGround >= 0.0 ? (toTop > 0.0 ? min(toTop, toGround) : toGround) : toTop;
    if (distance <= 0.0)
        return result;

    float cosine = dot(direction, sunDirection);
    float airPhase = usesPhase ? phaseOfAir(cosine) : phaseOfNothing;
    float dustPhase = usesPhase ? phaseOfDust(cosine) : phaseOfNothing;

    // How much of the light gathered further along still arrives at the start of the ray.
    float3 arriving = float3(1.0, 1.0, 1.0);
    float stepLength = distance / float(stepCount);
    for (int step = 0; step < stepCount; ++step)
    {
        // A little before the middle of the step, where the average of what a step gathers lies.
        float3 position = origin + direction * ((float(step) + 0.3) * stepLength);
        float radius = length(position);
        float3 up = position / radius;
        float sunUpwards = dot(up, sunDirection);

        Medium medium = mediumAt(position);
        float3 throughStep = exp(-medium.extinction * stepLength);

        // The sun as it arrives here, unless the planet is in the way.
        float3 sunlight = transmittanceToTop(radius, sunUpwards);
        if (distanceToSphere(position - up * aboveGround, sunDirection, groundRadius()) >= 0.0)
            sunlight = float3(0.0, 0.0, 0.0);

        float3 sent = sunlight * (medium.scatteringByAir * airPhase + medium.scatteringByDust * dustPhase);
        if (usesMultipleScattering)
            sent += multipleScattering(radius, sunUpwards) * medium.scattering;

        // What a step sends is dimmed on its own way through the step. Summed up exactly over the
        // step, that is the following (Hillaire, "Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite").
        float3 extinction = max(medium.extinction, float3(1e-7, 1e-7, 1e-7));
        result.light += arriving * (sent - sent * throughStep) / extinction;
        result.scatteredAgain += arriving * (medium.scattering - medium.scattering * throughStep) / extinction;
        arriving *= throughStep;
    }

    if (lightsGround && toGround >= 0.0 && distance == toGround)
    {
        float3 position = origin + direction * distance;
        float3 up = normalize(position);
        float sunUpwards = dot(up, sunDirection);
        float3 sunlight = transmittanceToTop(groundRadius(), sunUpwards);
        result.light += arriving * sunlight * saturate(sunUpwards) * scene.atmosphere.groundAlbedo.rgb / pi;
    }
    return result;
}

// The sky view table is laid out around the camera: along x the angle to the sun as a compass bearing,
// finer towards the sun, and along y the angle to straight up, with the horizon in the middle and finer
// towards it from both sides. The sky changes fastest at the horizon and around the sun.
public void skyViewParameters(float2 uv, float radius, out float upwards, out float towardsSunBearing)
{
    uv = fromPixelMiddles(uv, skyViewTableSize);

    float ground = groundRadius();
    float belowHorizon = acos(sqrt(max(radius * radius - ground * ground, 0.0)) / radius);
    float horizonAngle = pi - belowHorizon;

    if (uv.y < 0.5)
    {
        float coordinate = 1.0 - 2.0 * uv.y;
        coordinate = 1.0 - coordinate * coordinate;
        upwards = cos(horizonAngle * coordinate);
    }
    else
    {
        float coordinate = uv.y * 2.0 - 1.0;
        upwards = cos(horizonAngle + belowHorizon * coordinate * coordinate);
    }

    towardsSunBearing = 1.0 - 2.0 * uv.x * uv.x;
}

float2 skyViewUv(bool meetsGround, float upwards, float towardsSunBearing, float radius)
{
    float ground = groundRadius();
    float belowHorizon = acos(sqrt(max(radius * radius - ground * ground, 0.0)) / radius);
    float horizonAngle = pi - belowHorizon;
    float angle = acos(clamp(upwards, -1.0, 1.0));

    float2 unit;
    if (!meetsGround)
    {
        float coordinate = 1.0 - sqrt(saturate(1.0 - angle / horizonAngle));
        unit.y = coordinate * 0.5;
    }
    else
    {
        float coordinate = sqrt(saturate((angle - horizonAngle) / belowHorizon));
        unit.y = coordinate * 0.5 + 0.5;
    }
    unit.x = sqrt(saturate(0.5 - 0.5 * towardsSunBearing));
    return toPixelMiddles(unit, skyViewTableSize);
}

// Where the camera is: straight above the planet's middle.
public float3 cameraInAtmosphere()
{
    return float3(0.0, scene.atmosphere.radii.z, 0.0);
}

// How bright the sun is before the air: its colour times its intensity.
float3 sunBrightness()
{
    return scene.lightColor.rgb * scene.lightDirection.w;
}

// The light of the sky from a direction of the world, as the camera sees it.
public float3 skyLight(float3 direction)
{
    float3 camera = cameraInAtmosphere();
    float3 sun = towardsSun();

    // The compass bearing of the direction against that of the sun. Straight up and down have none.
    float2 flat = direction.xz;
    float2 sunFlat = sun.xz;
    float bearing = 1.0;
    if (dot(flat, flat) > 1e-8 && dot(sunFlat, sunFlat) > 1e-8)
        bearing = dot(normalize(flat), normalize(sunFlat));

    bool meetsGround = distanceToSphere(camera, direction, groundRadius()) >= 0.0;
    float2 uv = skyViewUv(meetsGround, direction.y, bearing, camera.y);
    return textures[scene.atmosphere.tables.z].SampleLevel(samplers[scene.atmosphere.tables.w], uv, 0.0).rgb * sunBrightness();
}

// The disc of the sun itself, for a direction that looks at it and not at the ground. sunSize is the
// tangent of half the angle the sun takes up in the sky.
public float3 sunDisc(float3 direction, float sunSize)
{
    float3 camera = cameraInAtmosphere();
    float atEdge = 1.0 / sqrt(1.0 + sunSize * sunSize);
    float towards = dot(direction, towardsSun());

    // A soft rim of a tenth of the disc keeps its edge from stair-stepping.
    float inside = smoothstep(atEdge, lerp(atEdge, 1.0, 0.1), towards);
    if (inside <= 0.0 || distanceToSphere(camera, direction, groundRadius()) >= 0.0)
        return float3(0.0, 0.0, 0.0);

    // All of the sun's light comes from this small disc, so the disc is that much brighter than a
    // surface it lights. The picture cannot hold more than its format allows.
    float3 light = sunBrightness() * transmittanceToTop(camera.y, direction.y) / (pi * sunSize * sunSize);
    return min(light * inside, float3(30000.0, 30000.0, 30000.0));
}

// The sun's light as it arrives at the ground around the camera: dimmer and redder the lower the sun
// stands, and gone once it is below the horizon.
public float3 sunlightOnGround()
{
    float3 camera = cameraInAtmosphere();
    float3 sun = towardsSun();
    if (distanceToSphere(camera, sun, groundRadius()) >= 0.0)
        return float3(0.0, 0.0, 0.0);
    return sunBrightness() * transmittanceToTop(camera.y, sun.y);
}

// The planet's ground, for a direction that looks at it where the scene has nothing: lit by the sun
// like a flat, matt surface of the colour the atmosphere takes the ground to have. Without it, the
// world would end in black wherever the scene's own ground does.
public float3 groundLight(float3 direction)
{
    if (distanceToSphere(cameraInAtmosphere(), direction, groundRadius()) < 0.0)
        return float3(0.0, 0.0, 0.0);
    return scene.atmosphere.groundAlbedo.rgb / pi * sunlightOnGround() * saturate(towardsSun().y);
}

// The light of the sky and the planet's ground on a surface is kept as nine numbers per colour, which
// SkyLight.slang works out once per frame. Each belongs to one of nine simple shapes on the sphere
// (the first spherical harmonics): one that is the same everywhere, three that follow an axis, and
// five that follow products of two axes. The light for a way of facing is the nine numbers times
// what the nine shapes are in that direction, summed up.
public static const uint skyIrradianceCount = 9;

public void skyIrradianceBasis(float3 direction, out float basis[skyIrradianceCount])
{
    basis[0] = 0.282095;
    basis[1] = 0.488603 * direction.y;
    basis[2] = 0.488603 * direction.z;
    basis[3] = 0.488603 * direction.x;
    basis[4] = 1.092548 * direction.x * direction.y;
    basis[5] = 1.092548 * direction.y * direction.z;
    basis[6] = 0.315392 * (3.0 * direction.z * direction.z - 1.0);
    basis[7] = 1.092548 * direction.x * direction.z;
    basis[8] = 0.546274 * (direction.x * direction.x - direction.y * direction.y);
}

// All the light that the sky and the planet's ground send onto a surface that faces along the normal.
// The sun's own light is not part of it. Nothing stands in its way here: a surface under a roof gets
// as much as one in the open, until ambient occlusion takes that away.
public float3 skyIrradiance(float3 normal)
{
    float basis[skyIrradianceCount];
    skyIrradianceBasis(normal, basis);

    float3 light = float3(0.0, 0.0, 0.0);
    for (uint index = 0; index < skyIrradianceCount; ++index)
        light += scene.skyIrradiance[index].rgb * basis[index];

    // Nine numbers overshoot a little where the light changes quickly, which can dip below nothing.
    return max(light, float3(0.0, 0.0, 0.0));
}

// What shiny surfaces mirror of the sky is kept in one square picture of every direction, in several
// sizes, which SkyReflection.slang fills. The sphere of directions is folded flat like this: upwards
// lies in the middle of the square, the horizon on the diamond between the middles of its sides, and
// straight down in its four corners (an octahedral map). The fold's seams lie below the horizon, where
// the planet's ground is one colour, so they do not show.
// Has to match SkyReflectionMips in Atmosphere.h.
static const float skyReflectionMips = 5.0;

public float3 skyReflectionDirection(float2 uv)
{
    float2 folded = uv * 2.0 - 1.0;
    float3 direction = float3(folded.x, 1.0 - abs(folded.x) - abs(folded.y), folded.y);
    float below = saturate(-direction.y);
    direction.x += direction.x >= 0.0 ? -below : below;
    direction.z += direction.z >= 0.0 ? -below : below;
    return normalize(direction);
}

float2 skyReflectionUv(float3 direction)
{
    direction /= abs(direction.x) + abs(direction.y) + abs(direction.z);
    float2 folded = direction.xz;
    if (direction.y < 0.0)
        folded = (1.0 - abs(direction.zx)) * float2(direction.x >= 0.0 ? 1.0 : -1.0, direction.z >= 0.0 ? 1.0 : -1.0);
    return folded * 0.5 + 0.5;
}

// The light of the sky and the planet's ground as a surface of a roughness mirrors it from a direction:
// sharp for a smooth surface, blurred for a rough one. The sun's disc is not in it, the sun has its
// own highlight. Nothing stands in its way here either, see skyIrradiance.
public float3 skyReflection(float3 direction, float roughness)
{
    float size = roughness * (skyReflectionMips - 1.0);
    return textures[scene.skyReflection].SampleLevel(samplers[scene.skyReflectionSampler], skyReflectionUv(direction), size).rgb;
}
