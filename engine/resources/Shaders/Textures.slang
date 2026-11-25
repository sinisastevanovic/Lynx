// Every texture, every cube and every sampler the renderer knows, as tables that are bound once for the whole frame.
// Import it with "import Textures;". A shader gets the index of a texture and of a sampler through its
// push constants or from the data of what it draws, and reads with
//     textures[textureIndex].Sample(samplers[samplerIndex], uv)
// An index that differs between the pixels or instances of one draw has to be wrapped: textures[NonUniformResourceIndex(index)].
module Textures;

// The binding numbers have to match SharedBindings.h.
[vk::binding(1, 0)] public Texture2D textures[];
[vk::binding(2, 0)] public SamplerState samplers[];

// Samplers that compare what they read with a value instead of returning it, as shadow maps need.
// Used with SampleCmp. They have a table of their own because the shading language tells the two kinds apart.
[vk::binding(3, 0)] public SamplerComparisonState comparisonSamplers[];

// Every cube the renderer knows: six square pictures each, read by a direction instead of by a place,
//     cubeTextures[cubeIndex].SampleLevel(samplers[samplerIndex], direction, level)
// A cube has indices of its own, apart from those of the flat textures.
[vk::binding(4, 0)] public TextureCube cubeTextures[];

// The direction that a place on one face of a cube stands for, for a pass that draws into the faces.
// The faces are in Vulkan's order: +X, -X, +Y, -Y, +Z, -Z. uv runs from 0 to 1 across the face,
// from its top left as the face is stored.
public float3 cubeDirection(uint face, float2 uv)
{
    float2 onFace = uv * 2.0 - 1.0;
    float3 direction;
    switch (face)
    {
        case 0: direction = float3(1.0, -onFace.y, -onFace.x); break;
        case 1: direction = float3(-1.0, -onFace.y, onFace.x); break;
        case 2: direction = float3(onFace.x, 1.0, onFace.y); break;
        case 3: direction = float3(onFace.x, -1.0, -onFace.y); break;
        case 4: direction = float3(onFace.x, -onFace.y, 1.0); break;
        default: direction = float3(-onFace.x, -onFace.y, -1.0); break;
    }
    return normalize(direction);
}
