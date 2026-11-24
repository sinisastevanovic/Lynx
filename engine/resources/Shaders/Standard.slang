// Meshes with a standard material, as a game draws them. The shading itself is in StandardShading.slang.

import StandardShading;

[vk::push_constant] ConstantBuffer<StandardConstants> constants;

[shader("vertex")]
StandardVaryings vertexMain(StandardVertex input, uint instanceIndex : SV_VulkanInstanceID)
{
    return standardVertex(constants, input, instanceIndex);
}

[shader("fragment")]
float4 fragmentMain(StandardVaryings input, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    return standardFragment(constants, input, isFrontFace);
}
