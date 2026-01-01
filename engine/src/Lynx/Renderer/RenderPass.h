#pragma once
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"

namespace Lynx
{
    enum class RenderFlags : uint8_t
    {
        None = 0,
        MainPass = 1 << 0,
        ShadowPass = 1 << 1,
        All = MainPass | ShadowPass
    };
    inline RenderFlags operator|(RenderFlags a, RenderFlags b) { return (RenderFlags)((uint8_t)a | (uint8_t)b); }
    inline bool operator&(RenderFlags a, RenderFlags b) { return ((uint8_t)a & (uint8_t)b) != 0; }
    
    struct GPUInstanceData
    {
        glm::mat4 Model;
        int EntityID;
        float Padding[3];
    };

    struct BatchKey
    {
        StaticMesh* Mesh;
        uint32_t SubmeshIndex;
        Material* Material;
        RenderFlags RenderFlags;

        bool operator==(const BatchKey& other) const
        {
            return Mesh == other.Mesh && SubmeshIndex == other.SubmeshIndex && Material == other.Material && RenderFlags == other.RenderFlags;
        }
    };

    struct BatchKeyHasher
    {
        std::size_t operator()(const BatchKey& key) const
        {
            size_t h1 = std::hash<void*>()(key.Mesh);
            size_t h2 = std::hash<uint32_t>()(key.SubmeshIndex);
            size_t h3 = std::hash<void*>()(key.Material);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    struct BatchDrawCall
    {
        BatchKey Key;
        uint32_t FirstInstance;
        uint32_t InstanceCount;
    };
    
    struct PushData
    {
        glm::vec4 AlbedoColor;
        glm::vec4 EmissiveColorStrength;
        float MetallicStrength;
        float RoughnessStrength;
        float AlphaCutoff;
        float Padding;
    };

    struct SceneData
    {
        glm::mat4 ViewProjectionMatrix;
        glm::mat4 LightViewProjection;
        glm::vec4 CameraPosition;
        glm::vec4 LightDirection; // w is intensity
        glm::vec4 LightColor;
    };
    
    struct RenderCommand
    {
        std::shared_ptr<StaticMesh> Mesh;
        int SubmeshIndex;
        GPUInstanceData InstanceData; 
        float DistanceToCamera;
        int InstanceOffset = -1;
        RenderFlags Flags = RenderFlags::All;
    };

    struct RenderData
    {
        std::vector<RenderCommand> TransparentQueue;

        std::vector<BatchDrawCall> OpaqueDrawCalls;
        nvrhi::BufferHandle InstanceBuffer;

        glm::mat4 View;
        glm::mat4 Projection;
        glm::mat4 ViewProjection;
        glm::vec3 CameraPosition;

        glm::vec3 LightDirection;
        glm::vec3 LightColor;
        float LightIntensity;

        glm::mat4 LightViewProj; // TODO: Can't we use the normal ViewProjection for this?
        nvrhi::TextureHandle ShadowMap;
        nvrhi::SamplerHandle ShadowSampler;

        nvrhi::FramebufferHandle TargetFramebuffer;
        nvrhi::TextureHandle SceneColorInput;
        nvrhi::TextureHandle BloomTexture;

        bool ShowGrid = true;
        float BloomIntensity;
        bool FXAAEnabled; 

        uint32_t DrawCalls = 0;
        uint32_t IndexCount = 0;
    };

    struct RenderContext
    {
        nvrhi::DeviceHandle Device;
        nvrhi::CommandListHandle CommandList;

        nvrhi::FramebufferInfo PresentationFramebufferInfo;

        nvrhi::BufferHandle GlobalConstantBuffer;
        nvrhi::TextureHandle WhiteTexture;
        nvrhi::TextureHandle BlackTexture;
        nvrhi::TextureHandle NormalTexture;
        nvrhi::TextureHandle MetallicRoughnessTexture;

        std::function<nvrhi::SamplerHandle(SamplerSettings)> GetSampler;
    };

    struct MaterialCacheEntry
    {
        nvrhi::BindingSetHandle BindingSet;
        uint32_t Version;
    };
    

    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;
        virtual void Init(RenderContext& ctx) = 0;
        virtual void Execute(RenderContext& ctx, RenderData& renderData) = 0;
    };
}
