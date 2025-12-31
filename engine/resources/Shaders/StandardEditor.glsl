#type vertex
#version 450
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Tangent; // Changed to vec4
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_Color;

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec3 v_WorldPos;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec4 v_Tangent; // Changed to vec4
layout(location = 4) out vec4 v_VertexColor;
layout(location = 5) out vec4 v_ShadowCoord;
layout(location = 6) out flat int v_EntityID;
 
layout(set = 0, binding = 0) uniform UBO {
    mat4 u_ViewProjection;
    mat4 u_LightViewProjection;
    vec4 u_CameraPosition;
    vec4 u_LightDirection;
    vec4 u_LightColor;
} ubo;

struct InstanceData
{
    mat4 Model;
    int EntityID;
    float Padding[3];
};

layout(std430, set = 0, binding = 10) readonly buffer InstanceBuffer {
    InstanceData instances[];
} u_Instances;

layout(push_constant) uniform PushConsts {
    vec4 u_AlbedoColor;
    vec4 u_EmissiveColorStrength;
    float u_Metallic;
    float u_Roughness;
    float u_AlphaCutoff;
} push;

void main() {
    InstanceData data = u_Instances.instances[gl_InstanceIndex];

    v_TexCoord = a_TexCoord;
    v_VertexColor = a_Color;
    v_EntityID = data.EntityID;

    vec4 worldPos = data.Model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;

    // TODO: This is maybe needed?
    //mat3 normalMatrix = transpose(inverse(mat3(push.u_Model)));
    mat3 normalMatrix = mat3(data.Model);
    v_Normal = normalize(normalMatrix * a_Normal);

    // Pass tangent and its handedness
    v_Tangent.xyz = normalize(normalMatrix * a_Tangent.xyz);
    v_Tangent.w = a_Tangent.w;

    // Calculate shadow coordinate
    // Offset matrix to move from [-1, 1] to [0, 1]
    // We flip Y here (-0.5 scale) to match Vulkan's inverted Y in clip space vs Texture coords
    const mat4 biasMat = mat4(
        0.5, 0.0, 0.0, 0.0,
        0.0, -0.5, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.5, 0.5, 0.0, 1.0
    );

    v_ShadowCoord = (biasMat * ubo.u_LightViewProjection) * vec4(v_WorldPos, 1.0);

    gl_Position = ubo.u_ViewProjection * worldPos;
}

#type pixel
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_WorldPos;
layout(location = 2) in vec3 v_Normal;
layout(location = 3) in vec4 v_Tangent; // Changed to vec4
layout(location = 4) in vec4 v_VertexColor;
layout(location = 5) in vec4 v_ShadowCoord;
layout(location = 6) in flat int v_EntityID;

layout(location = 0) out vec4 outColor;
layout(location = 1) out int outEntityID;

layout(set = 0, binding = 0) uniform UBO {
    mat4 u_ViewProjection;
    mat4 u_LightViewProjection;
    vec4 u_CameraPosition;
    vec4 u_LightDirection;
    vec4 u_LightColor;
} ubo;

layout(set = 0, binding = 1) uniform texture2D u_ShadowMap;
layout(set = 0, binding = 2) uniform sampler u_ShadowSampler;

layout(push_constant) uniform PushConsts {
    vec4 u_AlbedoColor;
    vec4 u_EmissiveColorStrength;
    float u_Metallic;
    float u_Roughness;
    float u_AlphaCutoff;
} push;

layout(set = 1, binding = 0) uniform texture2D u_AlbedoMap;
layout(set = 1, binding = 1) uniform texture2D u_NormalMap;
layout(set = 1, binding = 2) uniform texture2D u_MetallicRoughnessMap;
layout(set = 1, binding = 3) uniform texture2D u_EmissiveMap;
layout(set = 1, binding = 4) uniform sampler u_Sampler;


const float PI = 3.14159265359;

float CalculateShadow(vec4 shadowCoord)
{
    // Perspective divide (not strictly needed for ortho, but good practice)
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;

    // Check if outside shadow map range
    if(projCoords.z > 1.0 || projCoords.z < 0.0) return 1.0;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    // PCF (Percentage Closer Filtering)
    // Sample 3x3 grid
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(sampler2DShadow(u_ShadowMap, u_ShadowSampler), 0);

    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            // texture() on samplerShadow returns 1.0 if lit, 0.0 if shadowed
            shadow += texture(sampler2DShadow(u_ShadowMap, u_ShadowSampler),
                              vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z));
        }
    }

    return shadow / 9.0;
}

// ... PBR Functions ...
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 1. Setup vectors
    vec3 N = normalize(v_Normal);
    
    vec3 T = normalize(v_Tangent.xyz);
    T = normalize(T - dot(T, N) * N);
    // Use the W component to flip the bitangent if needed
    vec3 B = cross(N, T) * v_Tangent.w;
    mat3 TBN = mat3(T, B, N);

    vec3 normalMap = texture(sampler2D(u_NormalMap, u_Sampler), v_TexCoord).rgb;
    normalMap = normalMap * 2.0 - 1.0;
    //normalMap.y *= -1.0;
    N = normalize(TBN * normalMap);
   

    vec3 V = normalize(ubo.u_CameraPosition.xyz - v_WorldPos);
    vec3 L = normalize(-ubo.u_LightDirection.xyz);
    vec3 H = normalize(V + L);

    // ... (Rest of PBR logic same as before) ...
    // 2. Fetch Texture Data
    vec4 albedoSample = texture(sampler2D(u_AlbedoMap, u_Sampler), v_TexCoord);
    if (push.u_AlphaCutoff >= 0.0)
    {
        if (albedoSample.a < push.u_AlphaCutoff)
            discard;
    }
    vec3 albedo = pow(albedoSample.rgb, vec3(2.2)) * push.u_AlbedoColor.rgb;

    vec4 mrSample = texture(sampler2D(u_MetallicRoughnessMap, u_Sampler), v_TexCoord);
    float metallic = mrSample.b * push.u_Metallic;
    float roughness = mrSample.g * push.u_Roughness;

    vec3 emissiveTex = texture(sampler2D(u_EmissiveMap, u_Sampler), v_TexCoord).rgb;
    vec3 emissive = emissiveTex * push.u_EmissiveColorStrength.rgb * push.u_EmissiveColorStrength.a;

    // 3. Cook-Torrance BRDF
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    float shadow = CalculateShadow(v_ShadowCoord);
    //outColor = vec4(vec3(shadow), 1.0);
    //return;

    vec3 Lo = (kD * albedo / PI + specular) * ubo.u_LightColor.rgb * ubo.u_LightDirection.w * NdotL * shadow;

    // 4. Final Color
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo + emissive;

    outColor = vec4(color, albedoSample.a);
    //outColor = vec4(N * 0.5 + 0.5, 1.0);
    //outColor = vec4(T * 0.5 + 0.5, 1.0);
    //outColor = vec4(vec3(v_Tangent.w * 0.5 + 0.5), 1.0);
    outEntityID = v_EntityID;
}