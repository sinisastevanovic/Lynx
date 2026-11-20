// The editor's ground grid: lines every metre on the plane y = 0, with the X axis in red and the Z axis in blue.
// It is drawn on top of the finished picture, so its colours are what the screen shows, not light in the scene.

import Scene;

struct GridConstants
{
    float4x4 inverseViewProjection;
};
[vk::push_constant] ConstantBuffer<GridConstants> constants;

struct VertexOutput
{
    float4 position : SV_Position;

    // Where the pixel's view ray crosses the near and the far plane, in world space.
    float3 nearPoint : NEAR_POINT;
    float3 farPoint : FAR_POINT;
};

struct FragmentOutput
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

static const float2 corners[6] = {
    float2(1, 1), float2(-1, -1), float2(-1, 1),
    float2(-1, -1), float2(1, 1), float2(1, -1)
};

float3 toWorld(float2 corner, float depth)
{
    float4 world = mul(constants.inverseViewProjection, float4(corner, depth, 1.0));
    return world.xyz / world.w;
}

// Two triangles that cover the screen, made from the vertex index alone.
[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    float2 corner = corners[vertexIndex];

    VertexOutput output;
    output.position = float4(corner, 0.0, 1.0);
    output.nearPoint = toWorld(corner, 0.0);
    output.farPoint = toWorld(corner, 1.0);
    return output;
}

static const float3 lineColor = float3(0.39, 0.39, 0.39);
static const float3 xAxisColor = float3(0.88, 0.0, 0.0);
static const float3 zAxisColor = float3(0.0, 0.0, 0.88);

// An axis is this many pixels wide.
static const float axisWidth = 1.5;

// The grid fades out between these distances from the camera, before its lines become
// thinner than a pixel and start to flicker.
static const float fadeStart = 10.0;
static const float fadeEnd = 100.0;

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    // Where the view ray hits the ground plane: near.y + t * (far.y - near.y) = 0.
    float t = -input.nearPoint.y / (input.farPoint.y - input.nearPoint.y);

    // The plane is behind the camera.
    if (t <= 0.0)
        discard;

    float3 worldPosition = input.nearPoint + t * (input.farPoint - input.nearPoint);

    // How far the pixel is from the nearest line, measured in pixels. That makes every line
    // one pixel wide with a soft edge, however far away it is.
    float2 coordinate = worldPosition.xz;
    float2 pixelSize = fwidth(coordinate);
    float2 distanceToLine = abs(frac(coordinate - 0.5) - 0.5) / pixelSize;
    float coverage = 1.0 - min(min(distanceToLine.x, distanceToLine.y), 1.0);

    float3 color = lineColor;
    if (abs(worldPosition.x) < pixelSize.x * axisWidth)
        color = zAxisColor;
    if (abs(worldPosition.z) < pixelSize.y * axisWidth)
        color = xAxisColor;

    float distanceToCamera = distance(scene.cameraPosition.xyz, worldPosition);
    float fade = 1.0 - saturate((distanceToCamera - fadeStart) / (fadeEnd - fadeStart));

    float alpha = coverage * fade;
    if (alpha <= 0.05)
        discard;

    // The depth of the point on the plane, not of the triangle, so that the scene hides the grid where it is in front.
    float4 clipPosition = mul(scene.viewProjection, float4(worldPosition, 1.0));

    FragmentOutput output;
    output.color = float4(color, alpha);
    output.depth = clipPosition.z / clipPosition.w;
    return output;
}
