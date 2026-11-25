// What every shader that draws into the scene knows about the frame: the camera and the sun.
// Import it with "import Scene;" and read the values through "scene".
module Scene;

// Have to match ShadowCascades.h.
public static const uint shadowCascadeCount = 4;
public static const uint shadowCascadeResolution = 2048;
public static const float shadowCascadeBlend = 0.1;

// The air above the ground, in kilometres and per kilometre of the way through it. Atmosphere.slang
// works with it. Has to match AtmosphereData in Atmosphere.h.
public struct AtmosphereConstants
{
    // What the air's molecules scatter at ground level in xyz, one over the height they thin out by in w.
    public float4 rayleigh;

    // Dust and droplets: what they scatter, what they swallow, one over their height, and how strongly
    // they scatter onwards.
    public float4 mie;

    // What ozone swallows.
    public float4 ozone;
    public float4 groundAlbedo;

    // From the middle of the planet: the ground, the top of the air, the camera.
    public float4 radii;

    // Where the texture table has the tables of transmittance, multiple scattering and the sky view,
    // and the sampler table the sampler they are read with.
    public uint4 tables;
};

// How bright the picture is made. ExposurePass keeps it up to date, see Exposure.slang.
// Has to match ExposureState in RenderPass.h.
public struct ExposureState
{
    // What the light of the scene is multiplied by before it becomes colours on the screen.
    public float exposure;

    // Zero until the automatic exposure has been set once, from where on it only moves gradually.
    public uint isAdapted;
};

// Has to match SceneData in RenderPass.h.
public struct SceneConstants
{
    public float4x4 viewProjection;

    // The same of the frame before, which says where something was on screen then. See screenMotion.
    public float4x4 previousViewProjection;
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

    public AtmosphereConstants atmosphere;

    // The light of the sky on surfaces, as nine numbers per colour. See skyIrradiance in Atmosphere.slang.
    public float4* skyIrradiance;

    // Where the table of cubes has the cube shiny surfaces mirror the sky from, and the sampler table
    // the sampler it is read with. See skyReflection in Atmosphere.slang.
    public uint skyReflection;
    public uint skyReflectionSampler;

    // How bright the picture is made, for the shaders that finish it.
    public ExposureState* exposure;
    public uint64_t padding;
};

// The binding number has to match SharedBindings.h.
[vk::binding(0, 0)] public ConstantBuffer<SceneConstants> scene;

// How far a point moved on screen since the last frame, in texture coordinates: where it is minus where
// it was. Both are given as x, y and w of the point's position in clip space, this frame's from
// scene.viewProjection and the last frame's from scene.previousViewProjection.
// What is drawn in the same way in both frames comes out as exactly zero.
public float2 screenMotion(float3 clip, float3 previousClip)
{
    return (clip.xy / clip.z - previousClip.xy / previousClip.z) * 0.5;
}
