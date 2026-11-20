// Every texture and every sampler the renderer knows, as two tables that are bound once for the whole frame.
// Import it with "import Textures;". A shader gets the index of a texture and of a sampler through its
// push constants or from the data of what it draws, and reads with
//     textures[textureIndex].Sample(samplers[samplerIndex], uv)
// An index that differs between the pixels or instances of one draw has to be wrapped: textures[NonUniformResourceIndex(index)].
module Textures;

// The binding numbers have to match SharedBindings.h.
[vk::binding(1, 0)] public Texture2D textures[];
[vk::binding(2, 0)] public SamplerState samplers[];
