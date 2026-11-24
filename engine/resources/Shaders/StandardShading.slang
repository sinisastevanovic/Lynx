// How a mesh with a standard material is lit: one sun with a shadow map, a metallic-roughness surface
// (the Cook-Torrance model with the GGX distribution) and a little constant light from everywhere.
// Standard.slang and StandardEditor.slang are the two programs made from it.
module StandardShading;

import Scene;
import Textures;
import Mesh;
import Material;

// The same for every draw of the pass. What differs between draws comes from the instance and its material.
public struct StandardConstants
{
    // The instances of the scene, the places of those this frame draws, and the material table.
    public MeshInstance* instances;
    public uint* visibleInstances;
    public StandardMaterial* materials;

    // Indices into the texture table and the table of comparing samplers.
    public uint shadowMap;
    public uint shadowSampler;
};

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

    // Where the pixel lies in the shadow map: xy in texture coordinates, z as the depth to compare.
    public float4 shadowCoordinate : SHADOWCOORDINATE;
    public nointerpolation int entityId : ENTITYID;
    public nointerpolation uint material : MATERIAL;
};

// From the sun's clip space, where x and y run from -1 to 1, to texture coordinates from 0 to 1.
// Both start at the top left, so nothing is turned around.
static const float4x4 clipToTexture = float4x4(
    0.5,  0.0, 0.0, 0.5,
    0.0,  0.5, 0.0, 0.5,
    0.0,  0.0, 1.0, 0.0,
    0.0,  0.0, 0.0, 1.0);

public StandardVaryings standardVertex(StandardConstants constants, StandardVertex input, uint instanceIndex, uint vertexIndex)
{
    MeshInstance instance = meshInstance(constants.instances, constants.visibleInstances, instanceIndex);
    float3 inMesh = leafPosition(instance, input.position, vertexIndex, drawnLevel(constants.visibleInstances, instanceIndex));
    MeshPosition position = meshPosition(instance, inMesh);

    StandardVaryings output;
    output.position = position.clip;
    output.uv = input.uv;
    output.worldPosition = position.world.xyz;
    output.normal = normalize(meshNormal(instance, input.normal));
    output.tangent = float4(normalize(meshDirection(instance, input.tangent.xyz)), input.tangent.w);
    output.shadowCoordinate = mul(mul(clipToTexture, scene.lightViewProjection), float4(output.worldPosition, 1.0));
    output.entityId = instance.entityId;
    output.material = instance.material;
    return output;
}

static const float pi = 3.14159265359;

// How much of the sun reaches the pixel: 1 for all of it, 0 for none. Nine comparisons around the pixel
// are averaged, which softens the edge of a shadow.
float sunVisibility(StandardConstants constants, float4 shadowCoordinate)
{
    float3 coordinate = shadowCoordinate.xyz / shadowCoordinate.w;

    // Outside of what the shadow map covers, everything is lit.
    if (coordinate.z > 1.0 || coordinate.z < 0.0)
        return 1.0;
    if (coordinate.x < 0.0 || coordinate.x > 1.0 || coordinate.y < 0.0 || coordinate.y > 1.0)
        return 1.0;

    uint width, height;
    textures[constants.shadowMap].GetDimensions(width, height);
    float2 texelSize = 1.0 / float2(width, height);

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 uv = coordinate.xy + float2(x, y) * texelSize;
            visibility += textures[constants.shadowMap].SampleCmp(comparisonSamplers[constants.shadowSampler], uv, coordinate.z);
        }
    }
    return visibility / 9.0;
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

// isFrontFace is false for a pixel of a triangle that is seen from behind, which only a two-sided
// material lets through.
public float4 standardFragment(StandardConstants constants, StandardVaryings input, bool isFrontFace)
{
    StandardMaterial material = constants.materials[input.material];

    // The normal map tilts the surface's normal. It is stored relative to the surface, so it is
    // brought into the world with the surface's own three directions.
    float3 normal = normalize(input.normal);
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

    float3 toEye = normalize(scene.cameraPosition.xyz - input.worldPosition);
    float3 toLight = normalize(-scene.lightDirection.xyz);
    float3 halfway = normalize(toEye + toLight);

    float4 albedoSample = sampleMaterialTexture(material.albedoTexture, material.materialSampler, input.uv);
    if (material.alphaCutoff >= 0.0)
    {
        if (albedoSample.a < material.alphaCutoff)
            discard;
    }
    float3 albedo = albedoSample.rgb * material.albedoColor.rgb;

    float4 metallicRoughness = sampleMaterialTexture(material.metallicRoughnessTexture, material.materialSampler, input.uv);
    float metallic = metallicRoughness.b * material.metallic;

    // A roughness of zero is a perfect mirror, whose highlight is a single point of infinite brightness:
    // the formula below divides zero by zero there. No real surface is that smooth, so it is kept away from zero.
    float roughness = max(metallicRoughness.g * material.roughness, 0.045);

    float3 emissive = sampleMaterialTexture(material.emissiveTexture, material.materialSampler, input.uv).rgb
        * material.emissiveColorStrength.rgb * material.emissiveColorStrength.a;

    // A metal reflects in its own colour, everything else reflects a little white.
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 fresnel = fresnelSchlick(max(dot(halfway, toEye), 0.0), f0);

    float distribution = distributionGGX(normal, halfway, roughness);
    float geometry = geometrySmith(normal, toEye, toLight, roughness);

    float3 numerator = distribution * geometry * fresnel;
    float denominator = 4.0 * max(dot(normal, toEye), 0.0) * max(dot(normal, toLight), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    // What is not reflected enters the surface and comes back out in its colour, except in metals.
    float3 diffuseShare = (float3(1.0, 1.0, 1.0) - fresnel) * (1.0 - metallic);

    float nDotL = max(dot(normal, toLight), 0.0);
    float visibility = sunVisibility(constants, input.shadowCoordinate);

    float3 sunLight = (diffuseShare * albedo / pi + specular) * scene.lightColor.rgb * scene.lightDirection.w * nDotL * visibility;
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo;

    return float4(ambient + sunLight + emissive, albedoSample.a);
}
