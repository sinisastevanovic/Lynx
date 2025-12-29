#pragma once
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"

namespace Lynx
{
    struct PushData
    {
        glm::mat4 Model;
        glm::vec4 Color;
        float AlphaCutoff;
        int EntityID;
        float Padding[2];
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
        glm::mat4 Transform;
        glm::vec4 Color;
        int EntityID;
        float DistanceToCamera;
    };

    struct RenderData
    {
        std::vector<RenderCommand> OpaqueQueue;
        std::vector<RenderCommand> TransparentQueue;

        glm::mat4 ViewProjection;
        glm::vec3 CameraPosition;

        glm::vec3 LightDirection;
        glm::vec3 LightColor;
        float LightIntensity;

        glm::mat4 LightViewProj; // TODO: Can't we use the normal ViewProjection for this?
        nvrhi::TextureHandle ShadowMap;
        nvrhi::SamplerHandle ShadowSampler;

        nvrhi::FramebufferHandle TargetFramebuffer;
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

    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;
        virtual void Init(RenderContext& ctx) = 0;
        virtual void Execute(RenderContext& ctx, RenderData& renderData) = 0;
    };
}
