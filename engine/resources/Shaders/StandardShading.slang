// How a mesh with a standard material is lit: one sun with a soft shadow in cascades, a metallic-roughness surface
// (the Cook-Torrance model with the GGX distribution) and the light of the sky.
// Standard.slang and StandardEditor.slang are the two programs made from it.
module StandardShading;

import Scene;
import Textures;
import Mesh;
import Material;
import Atmosphere;

// The same for every draw of the pass. What differs between draws comes from the instance and its material.
public struct StandardConstants
{
    // The instances of the scene, where they are and where they were in the last frame, the places of
    // those this frame draws, and the material table.
    public MeshInstance* instances;
    public MeshTransform* transforms;
    public MeshTransform* previousTransforms;
    public uint* visibleInstances;
    public StandardMaterial* materials;

    // What the view shows, one of the numbers below, and whether materials with cut-outs are cut out.
    public uint viewMode;
    public uint showCutOuts;
};

// Has to match ViewMode in ViewMode.h.
public static const uint viewLit = 0;
public static const uint viewWireframe = 1;
public static const uint viewUnlit = 2;
public static const uint viewLightingOnly = 3;
public static const uint viewNormals = 4;
public static const uint viewLevelOfDetail = 5;
public static const uint viewShadowCascades = 6;

public struct StandardVertex
{
    public float3 position : POSITION;
    public float3 normal : NORMAL;

    // The direction in xyz. The sign in w says which way the third direction of the surface points.
    public float4 tangent : TANGENT;
    public float2 uv : TEXCOORD0;
};

public struct StandardVaryings
{
    public float4 position : SV_Position;
    public float2 uv : TEXCOORD0;
    public float3 worldPosition : WORLDPOSITION;
    public float3 normal : NORMAL;
    public float4 tangent : TANGENT;

    // Where the surface is on screen, and where it was in the last frame: x, y and w in clip space.
    public float3 clip : CLIP;
    public float3 previousClip : PREVIOUSCLIP;

    public nointerpolation int entityId : ENTITYID;
    public nointerpolation uint material : MATERIAL;

    // The level of detail the instance is drawn in, for the view that shows it.
    public nointerpolation float level : LEVEL;
};

public StandardVaryings standardVertex(StandardConstants constants, StandardVertex input, uint instanceIndex, uint vertexIndex)
{
    MeshInstance instance = meshInstance(constants.instances, constants.visibleInstances, instanceIndex);
    float level = drawnLevel(constants.visibleInstances, instanceIndex);
    float3 inMesh = leafPosition(instance, input.position, vertexIndex, level);
    float4x4 model = meshModel(constants.transforms, constants.visibleInstances, instanceIndex);
    MeshPosition position = meshPosition(model, inMesh);

    // Where the same vertex was a frame ago. Foliage is taken at the level it has now: a leaf that
    // grows or shrinks with the level does not count as moving.
    float4x4 previousModel = meshModel(constants.previousTransforms, constants.visibleInstances, instanceIndex);
    float4 previousClip = mul(scene.previousViewProjection, mul(previousModel, float4(inMesh, 1.0)));

    StandardVaryings output;
    output.position = position.clip;
    output.uv = input.uv;
    output.worldPosition = position.world.xyz;
    output.normal = normalize(meshNormal(model, input.normal));
    output.tangent = float4(normalize(meshDirection(model, input.tangent.xyz)), input.tangent.w);
    output.clip = position.clip.xyw;
    output.previousClip = previousClip.xyw;
    output.entityId = instance.entityId;
    output.material = instance.material;
    output.level = level;
    return output;
}

// A colour that is meant to reach the screen as it is, for the views that show one thing about a surface.
// Such a view is shown without tone mapping, but the picture is still brought from linear light to what a
// screen expects, which this undoes beforehand.
float fromScreen(float shown)
{
    return shown <= 0.04045 ? shown / 12.92 : pow((shown + 0.055) / 1.055, 2.4);
}

float3 shownAsItIs(float3 colour)
{
    colour = saturate(colour);
    return float3(fromScreen(colour.r), fromScreen(colour.g), fromScreen(colour.b));
}

// A colour for a level of detail: green for the finest, through yellow to red for the eighth and beyond.
// Every other level is a little darker, so that neighbouring levels can be told apart.
float3 levelColour(float level)
{
    float coarseness = saturate(level / 8.0);
    float3 colour = float3(saturate(2.0 * coarseness), saturate(2.0 - 2.0 * coarseness), 0.1);
    return (uint(level) & 1) == 1 ? colour * 0.7 : colour;
}

// Which cascade of the sun's shadow a place takes its shadow from, and where it lies in that cascade's map.
struct CascadePlace
{
    // shadowCascadeCount for a place outside all cascades.
    uint cascade;

    // From 0 in the middle of the map to 1 on its edge.
    float towardsEdge;
};

// A place takes its shadow from the first cascade whose map it lies in, which is the one with the finest
// detail there. Where the maps lie is known as the sun sees the world, so one matrix serves all of them.
CascadePlace cascadePlaceOf(float3 worldPosition)
{
    float2 seenBySun = mul(scene.sunView, float4(worldPosition, 1.0)).xy;

    CascadePlace place;
    place.cascade = shadowCascadeCount;
    place.towardsEdge = 0.0;
    for (uint cascade = 0; cascade < shadowCascadeCount; ++cascade)
    {
        float2 fromMiddle = abs(seenBySun - scene.cascadeFootprint[cascade].xy) * scene.cascadeFootprint[cascade].z;
        float towardsEdge = max(fromMiddle.x, fromMiddle.y);
        if (towardsEdge < 1.0)
        {
            place.cascade = cascade;
            place.towardsEdge = towardsEdge;
            break;
        }
    }
    return place;
}

// The sun is a disc in the sky, not a point, so the edge of a shadow is soft: sharp where the thing that
// casts it touches the ground, and wider the farther the shadow has travelled. A map holds one depth per
// texel and knows nothing of that, so the edge is made here, per pixel, in two steps
// ("Percentage-Closer Soft Shadows", Fernando 2005):
//   1. Depths of the map around the place are read. Those nearer to the sun are what casts the shadow,
//      and their average distance to the place says how wide the edge is here.
//   2. Comparisons are averaged over a disc of that width.
// Both use the same points, spread evenly over a disc and turned by a different angle in every pixel:
// a pattern that repeated from pixel to pixel would show as bands, a turned one shows as fine grain.
static const uint blockerTaps = 8;
static const uint filterTaps = 16;

// The edge is never narrower than this, in texels from its middle to its rim, or the texels would show.
// And never wider, or the points of the disc would lie too far apart to find what is between them.
static const float narrowestEdge = 1.5;
static const float widestEdge = 12.0;

static const float pi = 3.14159265359;

// A number from 0 to 1 that differs from pixel to pixel with no pattern the eye picks up
// ("interleaved gradient noise", Jimenez 2014).
float noiseOfPixel(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

// The direction of the next point of a disc from the one before: turned by the golden angle, which never
// lines points up. With the distance below, the points cover the disc evenly, however many there are.
float2 turnedOn(float2 direction)
{
    static const float cosine = -0.73736888;
    static const float sine = 0.67549029;
    return float2(direction.x * cosine - direction.y * sine, direction.x * sine + direction.y * cosine);
}

// How far from the middle of a disc of radius one the point with this number lies.
float fromMiddle(uint point, uint count)
{
    return sqrt((float(point) + 0.5) / float(count));
}

// How much of the sun reaches a place according to one cascade: 1 for all of it, 0 for none.
// The normal is that of the surface itself, without its normal map. turn is the angle for this pixel.
float cascadeVisibility(uint cascade, float3 worldPosition, float3 normal, float turn)
{
    // A texel of the map covers a patch of the surface but holds one depth, so a surface at a slant to
    // the sun would shadow itself in stripes. The place is looked up a little off the surface, by as much
    // as the texels of this cascade are large.
    float texelInWorld = scene.cascadeFootprint[cascade].w;
    float3 offSurface = worldPosition + normal * (texelInWorld * 2.0);
    float3 coordinate = mul(scene.cascadeToTexture[cascade], float4(offSurface, 1.0)).xyz;

    // Beyond what the map covers in depth, everything is lit. To its sides the sampler says so.
    if (coordinate.z > 1.0 || coordinate.z < 0.0)
        return 1.0;

    Texture2D map = textures[NonUniformResourceIndex(scene.cascadeTextures[cascade])];
    float texel = 1.0 / float(shadowCascadeResolution);
    float depthInWorld = scene.cascadeDepth[cascade];

    // A point of the disc lies beside the place, and the surface is there too, at another depth unless
    // it faces the sun squarely. Comparing with the depth of the place itself would find the surface in
    // its own shadow, the more so the wider the disc. So every point is compared with the depth the
    // surface has there, which follows from the way it faces: by this much per texel to the side.
    // A surface the sun only grazes would need more than is sensible, so it gets a limit.
    float3 seenBySun = mul(scene.sunView, float4(normal, 0.0)).xyz;
    float2 depthPerTexel = float2(seenBySun.x, -seenBySun.y) / max(seenBySun.z, 0.25) * (texelInWorld / depthInWorld);

    // What casts a shadow here, and from how far. A read fetches the four texels around its point.
    float searchRadius = clamp(scene.shadowWidestEdge / texelInWorld, narrowestEdge, widestEdge);
    float2 direction = float2(cos(turn), sin(turn));
    float blockers = 0.0;
    float depthToBlockers = 0.0;
    for (uint tap = 0; tap < blockerTaps; ++tap)
    {
        float2 offset = direction * (fromMiddle(tap, blockerTaps) * searchRadius);
        direction = turnedOn(direction);

        float surface = coordinate.z + dot(depthPerTexel, offset);
        float4 depths = map.GatherRed(samplers[scene.shadowDepthSampler], coordinate.xy + offset * texel);
        float4 isBlocker = float4(depths < float4(surface, surface, surface, surface));
        blockers += dot(isBlocker, float4(1.0, 1.0, 1.0, 1.0));
        depthToBlockers += dot(isBlocker, surface - depths);
    }

    // Nothing in the way: lit. Everything in the way: deep inside a shadow, where the second step
    // would find the same.
    if (blockers == 0.0)
        return 1.0;
    if (blockers == float(4 * blockerTaps))
        return 0.0;

    // The width of the edge grows with the distance the shadow has travelled, by the size of the sun.
    float edgeInWorld = depthToBlockers / blockers * depthInWorld * scene.shadowSoftening;
    float radius = clamp(edgeInWorld / texelInWorld, narrowestEdge, searchRadius);

    direction = float2(cos(turn), sin(turn));
    float visibility = 0.0;
    for (uint tap = 0; tap < filterTaps; ++tap)
    {
        float2 offset = direction * (fromMiddle(tap, filterTaps) * radius);
        direction = turnedOn(direction);

        float surface = coordinate.z + dot(depthPerTexel, offset);
        visibility += map.SampleCmpLevelZero(comparisonSamplers[scene.shadowSampler], coordinate.xy + offset * texel, surface);
    }
    return visibility / float(filterTaps);
}

// How much of the sun reaches a place: 1 for all of it, 0 for none. Outside the last cascade there
// is no shadow. Near the edge of a cascade's map its shadow is mixed with that of the next one, so that
// no line shows where the detail of the shadow changes.
// The normal is that of the surface itself, without its normal map. pixel is where the place is on screen.
float sunVisibility(float3 worldPosition, float3 normal, float2 pixel)
{
    CascadePlace place = cascadePlaceOf(worldPosition);
    if (place.cascade == shadowCascadeCount)
        return 1.0;

    float turn = noiseOfPixel(pixel) * 2.0 * pi;
    float visibility = cascadeVisibility(place.cascade, worldPosition, normal, turn);

    float intoBlend = (place.towardsEdge - (1.0 - shadowCascadeBlend)) / shadowCascadeBlend;
    if (intoBlend > 0.0)
    {
        float next = place.cascade + 1 < shadowCascadeCount ? cascadeVisibility(place.cascade + 1, worldPosition, normal, turn) : 1.0;
        visibility = lerp(visibility, next, saturate(intoBlend));
    }
    return visibility;
}

// A colour per cascade of the sun's shadow, from the nearest: red, green, blue, yellow. Grey beyond the last.
float3 cascadeColour(uint cascade)
{
    static const float3 colours[5] = { float3(1.0, 0.25, 0.25), float3(0.25, 1.0, 0.25), float3(0.3, 0.45, 1.0), float3(1.0, 1.0, 0.25), float3(0.5, 0.5, 0.5) };
    return colours[min(cascade, 4u)];
}

// How many of the surface's tiny mirrors face halfway between the light and the eye.
float distributionGGX(float3 normal, float3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfway), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = (nDotH2 * (a2 - 1.0) + 1.0);
    return a2 / (pi * denominator * denominator);
}

float geometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / (nDotV * (1.0 - k) + k);
}

// How much of those mirrors is hidden by their neighbours, seen from the eye and from the light.
float geometrySmith(float3 normal, float3 toEye, float3 toLight, float roughness)
{
    float nDotV = max(dot(normal, toEye), 0.0);
    float nDotL = max(dot(normal, toLight), 0.0);
    return geometrySchlickGGX(nDotL, roughness) * geometrySchlickGGX(nDotV, roughness);
}

// How much is reflected instead of entering the surface: more at flat angles.
float3 fresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// How much of the light that arrives from all around a surface it mirrors towards the eye, for a
// surface that mirrors f0 of it head on. A fitted curve in place of a table to look up
// (the second half of the split sum, after "Physically Based Shading on Mobile", Karis, 2014).
float3 mirroredShare(float3 f0, float roughness, float nDotV)
{
    static const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    static const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nDotV)) * r.x + r.y;
    float2 scaleAndBias = float2(-1.04, 1.04) * a004 + r.zw;
    return f0 * scaleAndBias.x + scaleAndBias.y;
}

// What the light needs to know about a place on a surface.
public struct LitSurface
{
    public float3 position;

    // The way the surface faces, with its normal map, and without it: its shadow is looked up by the latter.
    public float3 normal;
    public float3 faceNormal;

    public float3 albedo;
    public float metallic;
    public float roughness;
};

// The light a surface sends towards the eye: what it takes from the sun, shadowed, and from the sky.
// Everything that is lit goes through here, meshes and terrain alike. pixel is where the place is on screen.
public float3 lightOn(LitSurface surface, float2 pixel)
{
    float3 toEye = normalize(scene.cameraPosition.xyz - surface.position);
    float3 toLight = normalize(-scene.lightDirection.xyz);
    float3 halfway = normalize(toEye + toLight);

    // A metal reflects in its own colour, everything else reflects a little white.
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), surface.albedo, surface.metallic);
    float3 fresnel = fresnelSchlick(max(dot(halfway, toEye), 0.0), f0);

    float distribution = distributionGGX(surface.normal, halfway, surface.roughness);
    float geometry = geometrySmith(surface.normal, toEye, toLight, surface.roughness);

    float3 numerator = distribution * geometry * fresnel;
    float denominator = 4.0 * max(dot(surface.normal, toEye), 0.0) * max(dot(surface.normal, toLight), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    // What is not reflected enters the surface and comes back out in its colour, except in metals.
    float3 diffuseShare = (float3(1.0, 1.0, 1.0) - fresnel) * (1.0 - surface.metallic);

    float nDotL = max(dot(surface.normal, toLight), 0.0);
    // A surface that faces away from the sun is dark whatever its shadow says.
    float visibility = nDotL > 0.0 ? sunVisibility(surface.position, surface.faceNormal, pixel) : 0.0;

    // The sun as it arrives through the air: dimmer and redder the lower it stands.
    float3 sunLight = (diffuseShare * surface.albedo / pi + specular) * sunlightOnGround() * nDotL * visibility;
    // The light of the sky, from every direction the surface faces, comes back out in the surface's
    // colour. A metal has no such colour: it only mirrors.
    float3 ambient = surface.albedo * (1.0 - surface.metallic) / pi * skyIrradiance(surface.normal);

    // And what the surface mirrors of the sky: sharply if it is smooth, as a blur if it is rough.
    // A rough surface mirrors most from a direction between the mirrored one and its normal, the
    // rougher the nearer the normal. Looking the blur up along the mirrored direction instead would
    // put the bright horizon onto every rough floor ("Moving Frostbite to Physically Based Rendering",
    // Lagarde and de Rousiers, 2014).
    float3 mirrored = reflect(-toEye, surface.normal);
    float spread = surface.roughness * surface.roughness;
    float towardsMirrored = (1.0 - spread) * (sqrt(1.0 - spread) + spread);
    float3 mostMirrored = normalize(lerp(surface.normal, mirrored, towardsMirrored));

    // A normal map can tilt the mirrored direction into the surface itself, where there is no sky to see.
    float aboveSurface = saturate(1.0 + dot(mirrored, surface.faceNormal));
    ambient += skyReflection(mostMirrored, surface.roughness) * mirroredShare(f0, surface.roughness, max(dot(surface.normal, toEye), 0.0)) * (aboveSurface * aboveSurface);

    return ambient + sunLight;
}

// The colour of the edges in the view that draws the triangles' edges.
public float3 wireframeColour()
{
    return shownAsItIs(float3(0.8, 0.8, 0.8));
}

// The colour of a view that shows one thing about a surface in place of the lit picture: its own
// colour, the way it faces, the level of detail it is drawn in, or the cascade its shadow comes from.
// False for a view that shows light, which lightOn works out. Only the surface's position, normals and
// colour have to be known here. Everything that is lit goes through here too, so every view mode
// works for all of it alike.
public bool shownInPlaceOfLight(uint viewMode, LitSurface surface, float level, float2 pixel, out float3 colour)
{
    colour = float3(0.0, 0.0, 0.0);
    float3 toLight = normalize(-scene.lightDirection.xyz);

    if (viewMode == viewUnlit)
    {
        colour = surface.albedo;
        return true;
    }
    if (viewMode == viewNormals)
    {
        colour = shownAsItIs(surface.normal * 0.5 + 0.5);
        return true;
    }
    if (viewMode == viewLevelOfDetail)
    {
        float facingTheSun = max(dot(surface.normal, toLight), 0.0);
        colour = shownAsItIs(levelColour(level) * (0.35 + 0.65 * facingTheSun));
        return true;
    }
    if (viewMode == viewShadowCascades)
    {
        float lit = max(dot(surface.normal, toLight), 0.0) * sunVisibility(surface.position, surface.faceNormal, pixel);
        colour = shownAsItIs(cascadeColour(cascadePlaceOf(surface.position).cascade) * (0.35 + 0.65 * lit));
        return true;
    }
    return false;
}

// The surface as the view of the light alone shows it: every surface the same grey, and not a metal.
public LitSurface inLightAlone(LitSurface surface)
{
    surface.albedo = float3(0.5, 0.5, 0.5);
    surface.metallic = 0.0;
    return surface;
}

// How far the pixel's surface moved on screen since the last frame, see screenMotion.
public float2 standardMotion(StandardVaryings input)
{
    return screenMotion(input.clip, input.previousClip);
}

// isFrontFace is false for a pixel of a triangle that is seen from behind, which only a two-sided
// material lets through.
public float4 standardFragment(StandardConstants constants, StandardVaryings input, bool isFrontFace)
{
    StandardMaterial material = constants.materials[input.material];

    // The normal map tilts the surface's normal. It is stored relative to the surface, so it is
    // brought into the world with the surface's own three directions.
    float3 normal = normalize(input.normal);

    // The surface as it faces without its normal map, which is how its shadow is looked up.
    float3 faceNormal = isFrontFace ? normal : -normal;
    float3 tangent = normalize(input.tangent.xyz);
    tangent = tangent - dot(tangent, normal) * normal;

    // A mesh can come with a tangent that is useless at a pixel: one of no length, or one along the normal.
    // Normalising it would divide by zero, and bloom spreads such a pixel into a black square.
    // The normal map is left out there. The comparison is also false for a tangent that is not a number.
    if (length(tangent) > 0.0001)
    {
        tangent = normalize(tangent);
        float3 bitangent = cross(normal, tangent) * input.tangent.w;

        float3 mapped = sampleMaterialTexture(material.normalTexture, material.materialSampler, input.uv).rgb * 2.0 - 1.0;
        normal = normalize(tangent * mapped.x + bitangent * mapped.y + normal * mapped.z);
    }

    // A surface seen from behind is lit as what it is from there: one that faces the other way.
    if (!isFrontFace)
        normal = -normal;

    // The edges of the triangles are drawn whole and in one colour.
    if (constants.viewMode == viewWireframe)
        return float4(wireframeColour(), 1.0);

    float4 albedoSample = sampleMaterialTexture(material.albedoTexture, material.materialSampler, input.uv);
    if (material.alphaCutoff >= 0.0 && constants.showCutOuts != 0)
    {
        if (cutOutAlpha(material.albedoTexture, material.materialSampler, input.uv, albedoSample.a) < material.alphaCutoff)
            discard;
    }
    float3 albedo = albedoSample.rgb * material.albedoColor.rgb;

    LitSurface surface;
    surface.position = input.worldPosition;
    surface.normal = normal;
    surface.faceNormal = faceNormal;
    surface.albedo = albedo;

    // The views that show one thing about the surface instead of the lit picture.
    float3 shown;
    if (shownInPlaceOfLight(constants.viewMode, surface, input.level, input.position.xy, shown))
        return float4(shown, 1.0);

    // The light alone: not glowing either.
    bool isLightingOnly = constants.viewMode == viewLightingOnly;

    float4 metallicRoughness = sampleMaterialTexture(material.metallicRoughnessTexture, material.materialSampler, input.uv);
    float metallic = metallicRoughness.b * material.metallic;

    // A roughness of zero is a perfect mirror, whose highlight is a single point of infinite brightness:
    // the formula below divides zero by zero there. No real surface is that smooth, so it is kept away from zero.
    float roughness = max(metallicRoughness.g * material.roughness, 0.045);

    float3 emissive = sampleMaterialTexture(material.emissiveTexture, material.materialSampler, input.uv).rgb
        * material.emissiveColorStrength.rgb * material.emissiveColorStrength.a;

    surface.metallic = metallic;
    surface.roughness = roughness;
    if (isLightingOnly)
        surface = inLightAlone(surface);
    float3 light = lightOn(surface, input.position.xy);

    if (isLightingOnly)
        emissive = float3(0.0, 0.0, 0.0);

    return float4(light + emissive, albedoSample.a);
}
