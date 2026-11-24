// How the shaders that draw meshes get at a material. All materials are in one table on the GPU, an
// instance carries the index of its material, and a shader reads the material from the table.
// One draw call can therefore cover meshes with different materials.
module Material;

import Textures;

// One entry of the material table. Has to match MaterialTable::GpuMaterial in MaterialTable.h.
public struct StandardMaterial
{
    public float4 albedoColor;

    // The colour in rgb, the strength in a.
    public float4 emissiveColorStrength;
    public float metallic;
    public float roughness;

    // Pixels whose alpha is below this are left out. Negative for a material without cut-outs.
    public float alphaCutoff;

    // Indices into the texture and sampler tables. Albedo and emissive are the sRGB readings of their
    // textures, so sampling them gives linear light. All four are read with the one sampler.
    public uint albedoTexture;
    public uint normalTexture;
    public uint metallicRoughnessTexture;
    public uint emissiveTexture;
    public uint materialSampler;
};

// Reads one of a material's textures. The index comes from the table and may differ between the pixels
// of one draw, which the GPU has to be told.
public float4 sampleMaterialTexture(uint texture, uint materialSampler, float2 uv)
{
    return textures[NonUniformResourceIndex(texture)].Sample(samplers[NonUniformResourceIndex(materialSampler)], uv);
}

// By how much the alpha of a cut-out is raised for every size of its texture below the full one.
static const float alphaGainPerSize = 0.25;

// The alpha a cut-out is tested with, from the alpha that was read at uv.
//
// A texture seen from far away is read from one of its smaller sizes, in which a texel is the average
// of many. Of a twig with thin needles that average lies below the cut-off nearly everywhere, so the
// whole twig is left out, and a tree gets barer the farther away it is. Raising the alpha with the size
// that is read keeps about as much of the surface as its full size shows. It is a rule of thumb that
// is in wide use, not an exact repair: that would be to make the smaller sizes so that they keep the
// share of texels above the cut-off, where textures are cooked.
public float cutOutAlpha(uint texture, uint materialSampler, float2 uv, float alpha)
{
    float size = textures[NonUniformResourceIndex(texture)].CalculateLevelOfDetail(samplers[NonUniformResourceIndex(materialSampler)], uv);
    return alpha * (1.0 + max(size, 0.0) * alphaGainPerSize);
}

// True where a material with cut-outs leaves the surface out. For the passes that only draw depth:
// they read no more of the material than this needs.
public bool isCutOut(StandardMaterial* materials, uint material, float2 uv)
{
    float alphaCutoff = materials[material].alphaCutoff;
    if (alphaCutoff < 0.0)
        return false;

    uint texture = materials[material].albedoTexture;
    uint materialSampler = materials[material].materialSampler;
    float alpha = sampleMaterialTexture(texture, materialSampler, uv).a;
    return cutOutAlpha(texture, materialSampler, uv, alpha) < alphaCutoff;
}
