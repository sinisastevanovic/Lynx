// Meshes with a standard material, as the editor draws them: next to the colour, every pixel gets the ID
// of the entity it belongs to, which is what a click in the viewport picks by.
// The shading itself is in StandardShading.slang.

import StandardShading;

[vk::push_constant] ConstantBuffer<StandardConstants> constants;

// The targets in the order the render graph gives them to a pass: the colour, the motion, how much of
// the pixel is see-through, then the IDs. The pipeline decides which of the middle two is written.
struct FragmentOutput
{
    float4 color : SV_Target0;
    float2 motion : SV_Target1;
    float reactive : SV_Target2;
    int entityId : SV_Target3;
};

[shader("vertex")]
StandardVaryings vertexMain(StandardVertex input, uint instanceIndex : SV_VulkanInstanceID, uint vertexIndex : SV_VulkanVertexID)
{
    return standardVertex(constants, input, instanceIndex, vertexIndex);
}

[shader("fragment")]
FragmentOutput fragmentMain(StandardVaryings input, bool isFrontFace : SV_IsFrontFace)
{
    FragmentOutput output;
    output.color = standardFragment(constants, input, isFrontFace);
    output.motion = standardMotion(input);
    output.reactive = output.color.a;
    output.entityId = input.entityId;
    return output;
}
