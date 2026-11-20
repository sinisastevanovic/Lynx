// Coloured lines in world space: collider outlines and whatever game code draws through DebugRenderer.

import Scene;

struct VertexInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.color = input.color;
    output.position = mul(scene.viewProjection, float4(input.position, 1.0));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return input.color;
}
