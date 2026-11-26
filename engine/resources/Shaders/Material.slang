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

    // What a cut-out is tested from: the albedo texture, or only its finest sizes if its alpha cuts
    // shapes out (TextureSpecification::KeepsCoverage).
    public uint cutOutTexture;
    uint padding0;
    uint padding1;
    uint padding2;
};

// Reads one of a material's textures. The index comes from the table and may differ between the pixels
// of one draw, which the GPU has to be told.
public float4 sampleMaterialTexture(uint texture, uint materialSampler, float2 uv)
{
    return textures[NonUniformResourceIndex(texture)].Sample(samplers[NonUniformResourceIndex(materialSampler)], uv);
}

// True where a material with cut-outs leaves the surface out. For the passes that only draw depth:
// they read no more of the material than this needs.
//
// And for nothing else: what is drawn after the depth passes is drawn where its depth is the one
// they left, so it needs no test of its own.
//
// The alpha is read from the material's cutOutTexture. A texture seen from far away is read from one
// of its smaller sizes, in which a texel is the average of many, and of a twig that average lies
// below the cut-off nearly everywhere: trees get barer with distance. So for a texture whose alpha
// cuts shapes out, cutOutTexture holds its finest sizes only, see TextureSpecification::KeepsCoverage.
public bool isCutOut(StandardMaterial* materials, uint material, float2 uv)
{
    float alphaCutoff = materials[material].alphaCutoff;
    if (alphaCutoff < 0.0)
        return false;

    return sampleMaterialTexture(materials[material].cutOutTexture, materials[material].materialSampler, uv).a < alphaCutoff;
}
