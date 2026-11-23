// Makes one size of the depth pyramid: a picture of the scene's depth in ever smaller sizes, in which
// every pixel holds the farthest depth of all the pixels it stands for. Culling reads it to find out
// whether something lies wholly behind what is already drawn, with a handful of reads however large
// the thing is on screen.
//
// The scene's depth is reversed, so the farthest depth is the smallest value.
// The pass runs this once per size, each time from the size above it, and first from the scene's depth.

import Textures;

struct PyramidConstants
{
    // In pixels.
    int2 sourceSize;
    int2 targetSize;

    // Where the texture table has the source.
    uint sourceTexture;
};
[vk::push_constant] ConstantBuffer<PyramidConstants> constants;

// One triangle that covers the target, made from the vertex index alone.
[shader("vertex")]
float4 vertexMain(uint vertexIndex : SV_VertexID) : SV_Position
{
    float2 uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    return float4(uv * 2.0 - 1.0, 0.0, 1.0);
}

[shader("fragment")]
float fragmentMain(float4 position : SV_Position) : SV_Target
{
    // Every source pixel that the target pixel covers, also those it covers only in part. That is two
    // by two where the source is twice the size, and up to three by three where the sizes do not divide.
    // Leaving one out could hide something that is in fact seen through it.
    int2 target = int2(position.xy);
    int2 first = target * constants.sourceSize / constants.targetSize;
    int2 end = ((target + 1) * constants.sourceSize + constants.targetSize - 1) / constants.targetSize;

    float farthest = 1.0;
    for (int y = first.y; y < end.y; ++y)
    {
        for (int x = first.x; x < end.x; ++x)
            farthest = min(farthest, textures[constants.sourceTexture].Load(int3(x, y, 0)).r);
    }
    return farthest;
}
