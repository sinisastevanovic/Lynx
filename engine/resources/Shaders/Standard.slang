// Meshes with a standard material, as a game draws them. The shading itself is in StandardShading.slang.

import StandardShading;

[vk::push_constant] ConstantBuffer<StandardConstants> constants;

[shader("vertex")]
StandardVaryings vertexMain(StandardVertex input, uint instanceIndex : SV_VulkanInstanceID, uint vertexIndex : SV_VulkanVertexID)
{
    return standardVertex(constants, input, instanceIndex, vertexIndex);
}

// The targets in the order the render graph gives them to a pass: the colour, the motion, then how
// much of the pixel is see-through. The pipeline decides which of the last two is written.
struct FragmentOutput
{
    float4 color : SV_Target0;
    float2 motion : SV_Target1;
    float reactive : SV_Target2;
};

[shader("fragment")]
FragmentOutput fragmentMain(StandardVaryings input, bool isFrontFace : SV_IsFrontFace)
{
    FragmentOutput output;
    output.color = standardFragment(constants, input, isFrontFace);
    output.motion = standardMotion(input);
    output.reactive = output.color.a;
    return output;
}
