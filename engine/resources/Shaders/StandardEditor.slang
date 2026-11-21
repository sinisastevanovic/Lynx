// Meshes with a standard material, as the editor draws them: next to the colour, every pixel gets the ID
// of the entity it belongs to, which is what a click in the viewport picks by.
// The shading itself is in StandardShading.slang.

import StandardShading;

[vk::push_constant] ConstantBuffer<StandardConstants> constants;

struct FragmentOutput
{
    float4 color : SV_Target0;
    int entityId : SV_Target1;
};

[shader("vertex")]
StandardVaryings vertexMain(StandardVertex input, uint instanceIndex : SV_InstanceID)
{
    return standardVertex(constants, input, instanceIndex);
}

[shader("fragment")]
FragmentOutput fragmentMain(StandardVaryings input)
{
    FragmentOutput output;
    output.color = standardFragment(constants, input);
    output.entityId = input.entityId;
    return output;
}
