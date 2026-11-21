#pragma once
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <span>

#include "DebugRenderer.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"

namespace Lynx
{
    class GraphicsContext;
    class Shader;
    class SharedBindings;
    class UploadQueue;
    class UICanvas;

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

    // Where the texture and sampler tables have the textures of a material.
    struct MaterialTextureIndices
    {
        uint32_t Albedo = 0;
        uint32_t Normal = 0;
        uint32_t MetallicRoughness = 0;
        uint32_t Emissive = 0;
        uint32_t Sampler = 0;
    };

    struct BatchDrawCall
    {
        BatchKey Key;
        uint32_t FirstInstance;
        uint32_t InstanceCount;
        MaterialTextureIndices Textures;
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
        MaterialTextureIndices Textures;
    };

    struct ParticleInstanceData
    {
        glm::vec3 Position;
        float Rotation;
        glm::vec4 Color;
        float Size;
        float Life;
        float Padding[2];
    };

    struct ParticleBatch
    {
        Material* Material;
        uint32_t StartOffset;
        uint32_t Count;

        // Where the texture and sampler tables have the material's albedo texture.
        uint32_t AlbedoTexture;
        uint32_t AlbedoSampler;
    };

    struct BloomSettings
    {
        // Parts of the picture brighter than this glow. The knee makes the cut soft.
        float Threshold = 1.0f;
        float Knee = 0.1f;
        float Radius = 1.0f;

        // How much of the glow is added to the picture.
        float Intensity = 0.04f;
        bool Enabled = true;
    };

    struct RenderData
    {
        std::vector<RenderCommand> TransparentQueue;
        std::vector<BatchDrawCall> OpaqueDrawCalls;
        nvrhi::BufferHandle InstanceBuffer;

        std::vector<ParticleBatch> ParticleQueue;

        // The address shaders read the frame's mesh instances from, one GPUInstanceData each.
        uint64_t MeshInstanceAddress = 0;

        // The address shaders read the frame's particles from, one ParticleInstanceData each.
        uint64_t ParticleInstanceAddress = 0;

        // Views into the extracted scene. Valid only while it is being drawn.
        std::span<const DebugLine> DebugLines;
        std::span<const std::shared_ptr<UICanvas>> Canvases;

        glm::mat4 View;
        glm::mat4 Projection;
        glm::mat4 ViewProjection;
        glm::vec3 CameraPosition;

        glm::vec3 LightDirection;
        glm::vec3 LightColor;
        float LightIntensity;

        glm::mat4 LightViewProj; // TODO: Can't we use the normal ViewProjection for this?
        // The shadow map, and where the texture table and the table of comparing samplers have it and its sampler.
        nvrhi::TextureHandle ShadowMap;
        uint32_t ShadowMapIndex = 0;
        uint32_t ShadowSamplerIndex = 0;

        // Where the scene passes draw: the HDR picture, with depth and, in the editor, entity IDs.
        nvrhi::FramebufferHandle TargetFramebuffer;

        // Where the finished picture goes: the window, or the editor's viewport picture.
        nvrhi::FramebufferHandle OutputFramebuffer;

        nvrhi::TextureHandle SceneColorInput;

        // Where the texture table has the scene's HDR picture, and the bloom to add to it.
        // The bloom is a black picture unless the bloom pass has run in this frame.
        uint32_t SceneColorIndex = 0;
        uint32_t BloomTextureIndex = 0;

        BloomSettings Bloom;
        bool ShowGrid = true;
        bool FXAAEnabled; 

        uint32_t DrawCalls = 0;
        uint32_t IndexCount = 0;
    };

    struct RenderContext
    {
        // For passes that draw with Vulkan directly.
        GraphicsContext* Graphics = nullptr;
        UploadQueue* Uploads = nullptr;
        SharedBindings* Shared = nullptr;

        nvrhi::DeviceHandle Device;
        nvrhi::CommandListHandle CommandList;

        nvrhi::FramebufferInfo PresentationFramebufferInfo;
        nvrhi::FramebufferInfo FinalFramebufferInfo;

        nvrhi::BufferHandle GlobalConstantBuffer;
        nvrhi::TextureHandle WhiteTexture;
        nvrhi::TextureHandle BlackTexture;
        nvrhi::TextureHandle NormalTexture;
        nvrhi::TextureHandle MetallicRoughnessTexture;
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

    // The editor's scene target has a second color attachment that holds entity IDs for picking.
    // A pass whose shader does not output an ID has to leave it alone: without this,
    // every pixel the pass draws gets an undefined ID.
    inline void DisableEntityIdWrites(nvrhi::GraphicsPipelineDesc& pipelineDesc, const nvrhi::FramebufferInfo& framebufferInfo)
    {
        for (size_t target = 1; target < framebufferInfo.colorFormats.size(); ++target)
            pipelineDesc.renderState.blendState.targets[target].setColorWriteMask(static_cast<nvrhi::ColorMask>(0));
    }

    // Rebuilds a pass's pipelines whenever its shader is loaded or hot-reloaded.
    class PipelineState
    {
    public:
        // The path is relative to the engine resource directory, e.g. "Shaders/Bloom.glsl".
        void SetShader(const std::filesystem::path& engineShaderPath) { m_ShaderPath = engineShaderPath; m_Shader = nullptr; }
        bool Update(std::function<void(std::shared_ptr<Shader>)> builder);

    private:
        std::filesystem::path m_ShaderPath;
        std::shared_ptr<Shader> m_Shader;
        uint32_t m_Version = -1;
    };
}
