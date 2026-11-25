// What every shader that draws into the scene knows about the frame: the camera and the sun.
// Import it with "import Scene;" and read the values through "scene".
module Scene;

// Have to match ShadowCascades.h.
public static const uint shadowCascadeCount = 4;
public static const uint shadowCascadeResolution = 2048;
public static const float shadowCascadeBlend = 0.1;

// Has to match SceneData in RenderPass.h.
public struct SceneConstants
{
    public float4x4 viewProjection;
    public float4 cameraPosition;

    // The direction the light travels in. Its w is the intensity.
    public float4 lightDirection;
    public float4 lightColor;

    // The sun's shadow is kept in several maps, the cascades, each for a stretch of what the camera sees,
    // the nearest first. sunView is the world as the sun sees it, turned but not moved. Then per cascade:
    // from the world to where a place lies in its map (xy as texture coordinates, z as the depth to
    // compare), the square of ground its map covers as the sun sees it (its middle in xy, one over half
    // its width in z, and in w the size of a texel in the world), where the texture table has the map,
    // and how far the map reaches along the sun's light: its depths from 0 to 1 stand for that distance.
    public float4x4 sunView;
    public float4x4 cascadeToTexture[shadowCascadeCount];
    public float4 cascadeFootprint[shadowCascadeCount];
    public uint4 cascadeTextures;
    public float4 cascadeDepth;

    // Where the table of comparing samplers has the one the maps are compared with, and the table of
    // samplers the one their depths are read with.
    public uint shadowSampler;
    public uint shadowDepthSampler;

    // The edge of a shadow gets this much wider, from its middle to its rim, per unit of distance between
    // what casts the shadow and what receives it. And the widest it gets, in the scene's units.
    public float shadowSoftening;
    public float shadowWidestEdge;
};

// The binding number has to match SharedBindings.h.
[vk::binding(0, 0)] public ConstantBuffer<SceneConstants> scene;
