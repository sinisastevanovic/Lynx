// What every shader that draws into the scene knows about the frame: the camera and the sun.
// Import it with "import Scene;" and read the values through "scene".
module Scene;

// Has to match SceneData in RenderPass.h.
public struct SceneConstants
{
    public float4x4 viewProjection;
    public float4x4 lightViewProjection;
    public float4 cameraPosition;

    // The direction the light travels in. Its w is the intensity.
    public float4 lightDirection;
    public float4 lightColor;
};

// The binding number has to match SharedBindings.h.
[vk::binding(0, 0)] public ConstantBuffer<SceneConstants> scene;
