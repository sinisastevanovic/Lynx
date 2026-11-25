// Draws the patches of a terrain: the same small grid of triangles for every patch, stretched over the
// patch and lifted to the terrain's heights. See TerrainRenderer for which patches a frame draws.
// The depth pass and the shadow pass use the vertex shader alone, the colour pass both.

import Scene;
import Textures;
import Material;
import StandardShading;

// Has to match TerrainQuadsPerPatch in Terrain.h.
static const float quadsPerPatch = 32.0;
static const uint verticesPerSide = 33;

// A patch slides its vertices towards those of the next coarser level over this last share of the
// distance out to which its level is drawn.
static const float morphShare = 0.3;

// Has to match GpuPatch in TerrainRenderer.cpp.
struct TerrainPatch
{
    // The corner with the smallest x and z, counted from the terrain's own, and the length of the
    // patch the grid is stretched over.
    float2 origin;
    float size;

    // The level in the low 8 bits, the terrain's place in the list of terrains above them.
    uint levelAndTerrain;
};

// Has to match GpuTerrain in TerrainRenderer.cpp.
struct TerrainData
{
    float3 origin;
    float size;
    float lowest;
    float heightRange;
    uint heightTexture;
    uint heightSampler;
    float resolution;

    // How far from the camera the smallest patches are drawn. Each larger level reaches twice as far.
    float finestRange;

    // The ground: where the texture table has the picture of weights, how many layers it is mixed
    // from, and per layer its entry of the material table and how often its textures repeat per unit.
    uint weightTexture;
    uint layerCount;
    uint4 layerMaterials;
    float4 layerTilesPerUnit;
};

struct TerrainConstants
{
    // From the world to what the sun sees of a cascade, for the pass that draws into its shadow map.
    float4x4 sunViewProjection;

    TerrainPatch* patches;
    TerrainData* terrains;
    StandardMaterial* materials;

    // What the view shows, one of the view modes of StandardShading.
    uint viewMode;

    // Not zero for the pass that draws into a shadow map. The patches are as fine as the camera's
    // distance asks for either way.
    uint isSeenBySun;
};
[vk::push_constant] ConstantBuffer<TerrainConstants> constants;

struct TerrainVaryings
{
    float4 position : SV_Position;
    float3 worldPosition : WORLDPOSITION;
    nointerpolation uint terrain : TERRAIN;
    nointerpolation float level : LEVEL;
};

// The height of the terrain at a place counted from its corner, read from the given size of the heights.
float heightAt(TerrainData terrain, float2 place, float size)
{
    // The first and the last height lie on the terrain's edges, in the middles of the first and last texel.
    float2 uv = (place / terrain.size) * ((terrain.resolution - 1.0) / terrain.resolution) + 0.5 / terrain.resolution;
    float stored = textures[NonUniformResourceIndex(terrain.heightTexture)].SampleLevel(samplers[terrain.heightSampler], uv, size).r;
    return terrain.lowest + stored * terrain.heightRange;
}

[shader("vertex")]
TerrainVaryings vertexMain(uint vertexIndex : SV_VulkanVertexID, uint patchIndex : SV_VulkanInstanceID)
{
    TerrainPatch patch = constants.patches[patchIndex];
    uint level = patch.levelAndTerrain & 0xFF;
    uint terrainIndex = patch.levelAndTerrain >> 8;
    TerrainData terrain = constants.terrains[terrainIndex];

    // The vertex's place in the grid, from 0 to 1 each way.
    float2 inGrid = float2(vertexIndex % verticesPerSide, vertexIndex / verticesPerSide) / quadsPerPatch;
    float2 place = patch.origin + inGrid * patch.size;
    float3 world = terrain.origin + float3(place.x, heightAt(terrain, place, 0.0), place.y);

    // Towards the end of the distance its level is drawn to, every second vertex slides onto its
    // neighbour, so that the grid ends up as the grid of the next coarser level, which takes over there.
    float range = terrain.finestRange * exp2(float(level));
    float morph = saturate((distance(scene.cameraPosition.xyz, world) - (1.0 - morphShare) * range) / (morphShare * range));
    float2 toCoarser = frac(inGrid * (quadsPerPatch * 0.5)) * (2.0 / quadsPerPatch);
    place = patch.origin + (inGrid - toCoarser * morph) * patch.size;
    world = terrain.origin + float3(place.x, heightAt(terrain, place, 0.0), place.y);

    TerrainVaryings output;
    output.position = constants.isSeenBySun != 0 ? mul(constants.sunViewProjection, float4(world, 1.0)) : mul(scene.viewProjection, float4(world, 1.0));
    output.worldPosition = world;
    output.terrain = terrainIndex;
    output.level = float(level);
    return output;
}

[shader("fragment")]
float4 fragmentMain(TerrainVaryings input) : SV_Target
{
    if (constants.viewMode == viewWireframe)
        return float4(wireframeColour(), 1.0);

    TerrainData terrain = constants.terrains[input.terrain];
    float2 place = input.worldPosition.xz - terrain.origin.xz;

    // The way the ground faces, from the heights to both sides of the place. Far away a pixel covers
    // many heights, and a smaller size of them is read, or the ground would glitter.
    float spacing = terrain.size / (terrain.resolution - 1.0);
    float pixelInWorld = max(length(ddx(input.worldPosition.xz)), length(ddy(input.worldPosition.xz)));
    float smaller = max(log2(pixelInWorld / spacing), 0.0);
    float reach = spacing * exp2(smaller);
    float west = heightAt(terrain, place - float2(reach, 0.0), smaller);
    float east = heightAt(terrain, place + float2(reach, 0.0), smaller);
    float south = heightAt(terrain, place - float2(0.0, reach), smaller);
    float north = heightAt(terrain, place + float2(0.0, reach), smaller);
    float3 normal = normalize(float3(west - east, 2.0 * reach, south - north));

    LitSurface surface;
    surface.position = input.worldPosition;
    surface.normal = normal;
    surface.faceNormal = normal;

    if (terrain.layerCount == 0)
    {
        // A terrain without layers: grass where it is flat, rock where it is steep, so that its shape can be read.
        float steepness = saturate((1.0 - normal.y) * 3.0);
        surface.albedo = lerp(float3(0.13, 0.18, 0.07), float3(0.30, 0.28, 0.26), smoothstep(0.35, 0.65, steepness));
        surface.metallic = 0.0;
        surface.roughness = 0.9;
    }
    else
    {
        // How much of each layer shows here. The weights of the layers there are add up to one.
        float2 across = place / terrain.size;
        float4 weights = textures[NonUniformResourceIndex(terrain.weightTexture)].Sample(samplers[terrain.heightSampler], across);
        float total = 0.0;
        for (uint layer = 0; layer < terrain.layerCount; ++layer)
            total += weights[layer];
        if (total <= 0.0001)
        {
            weights = float4(1.0, 0.0, 0.0, 0.0);
            total = 1.0;
        }

        // The layers lie flat on the ground: their textures are read by where the place is in the
        // world. How fast that changes from pixel to pixel is worked out once, outside the loop, and
        // handed to every read: inside a loop that leaves layers out the GPU could not tell.
        float2 perPixelAcross = ddx(input.worldPosition.xz);
        float2 perPixelDown = ddy(input.worldPosition.xz);

        float3 albedo = float3(0.0, 0.0, 0.0);
        float3 tilt = float3(0.0, 0.0, 0.0);
        float metallic = 0.0;
        float roughness = 0.0;
        for (uint layer = 0; layer < terrain.layerCount; ++layer)
        {
            float share = weights[layer] / total;
            if (share <= 0.001)
                continue;

            StandardMaterial material = constants.materials[terrain.layerMaterials[layer]];
            float tiles = terrain.layerTilesPerUnit[layer];
            float2 uv = input.worldPosition.xz * tiles;
            SamplerState layerSampler = samplers[NonUniformResourceIndex(material.materialSampler)];

            float3 colour = textures[NonUniformResourceIndex(material.albedoTexture)].SampleGrad(layerSampler, uv, perPixelAcross * tiles, perPixelDown * tiles).rgb;
            float3 mapped = textures[NonUniformResourceIndex(material.normalTexture)].SampleGrad(layerSampler, uv, perPixelAcross * tiles, perPixelDown * tiles).rgb;
            float4 metallicRoughness = textures[NonUniformResourceIndex(material.metallicRoughnessTexture)].SampleGrad(layerSampler, uv, perPixelAcross * tiles, perPixelDown * tiles);

            albedo += share * colour * material.albedoColor.rgb;
            tilt += share * (mapped * 2.0 - 1.0);
            metallic += share * metallicRoughness.b * material.metallic;
            roughness += share * metallicRoughness.g * material.roughness;
        }

        // The layers' normal maps tilt the ground's normal. A texture's u runs along the world's x and
        // its v along z, so those two, laid onto the ground, are the directions the map is stored in.
        float3 alongU = normalize(float3(1.0, 0.0, 0.0) - normal * normal.x);
        float3 alongV = cross(alongU, normal);
        surface.normal = normalize(alongU * tilt.x + alongV * tilt.y + normal * tilt.z);
        surface.albedo = albedo;
        surface.metallic = metallic;

        // Kept away from zero like every other surface's, see StandardShading.
        surface.roughness = max(roughness, 0.045);
    }

    float3 shown;
    if (shownInPlaceOfLight(constants.viewMode, surface, input.level, input.position.xy, shown))
        return float4(shown, 1.0);

    if (constants.viewMode == viewLightingOnly)
        surface = inLightAlone(surface);
    return float4(lightOn(surface, input.position.xy), 1.0);
}
