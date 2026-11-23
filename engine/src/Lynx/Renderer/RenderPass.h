#pragma once
#include <glm/glm.hpp>

#include <filesystem>
#include <span>

#include "DebugRenderer.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/TextureSpecification.h"

struct VkCommandBuffer_T;

namespace Lynx
{
    class GeometryPool;
    class GpuBuffer;
    class GpuTexture;
    class GraphicsContext;
    class Shader;
    class SharedBindings;
    class UploadQueue;
    class UICanvas;

    // A run of draw commands in the buffer of a frame's draw commands.
    struct DrawRange
    {
        uint32_t First = 0;
        uint32_t Count = 0;
    };

    // What the camera sees is found in two phases, so that what lies behind other things need not be drawn.
    // Early: what was seen in the last frame, which is nearly everything that is seen in this one.
    // Late: what the depth of the early phase does not hide and was not drawn yet.
    enum class CullPhase : uint32_t
    {
        Early = 0,
        Late = 1
    };

    // The draw commands for the frame's meshes that are not see-through: one command per submesh and all
    // its instances, in a buffer on the GPU. A pass draws a whole range of them with one call.
    // Culling on the GPU decides how many instances each command draws, which may be none.
    struct MeshDrawCommands
    {
        const GpuBuffer* Buffer = nullptr;

        // What the camera sees, early and late, and what the sun sees, each split by whether the material
        // has cut-outs: only a material with cut-outs needs its texture where nothing but depth is drawn.
        // The four ranges of the camera follow each other directly, so they can be drawn as one range,
        // and so can the two of the sun.
        DrawRange CameraOpaque;
        DrawRange CameraCutOut;
        DrawRange CameraLateOpaque;
        DrawRange CameraLateCutOut;
        DrawRange ShadowOpaque;
        DrawRange ShadowCutOut;

        DrawRange CameraOpaqueOf(CullPhase phase) const { return phase == CullPhase::Early ? CameraOpaque : CameraLateOpaque; }
        DrawRange CameraCutOutOf(CullPhase phase) const { return phase == CullPhase::Early ? CameraCutOut : CameraLateCutOut; }

        // Everything the camera sees that is not see-through.
        DrawRange Camera() const { return { CameraOpaque.First, CameraOpaque.Count + CameraCutOut.Count + CameraLateOpaque.Count + CameraLateCutOut.Count }; }
    };

    // What culling on the GPU works with besides the instances, see GpuScene and CullPass.
    struct MeshCulling
    {
        // The frame's draw commands, into which culling counts the instances that are seen.
        uint64_t CommandAddress = 0;

        // Per batch of instances, the commands it is drawn with: the camera's early and late, and the sun's.
        uint64_t BatchAddress = 0;

        // The planes of the camera's view and of the sun's, and how the camera projects.
        uint64_t FrameAddress = 0;

        // Per place in the instance buffer, whether the camera saw its instance in the last frame.
        // Kept on the GPU from frame to frame, read by the early phase and written by the late one.
        uint64_t VisibilityAddress = 0;

        // How many places the instance buffer has. Zero if there is nothing to cull.
        uint32_t PlaceCount = 0;
    };

    // The scene's depth in ever smaller sizes, see DepthPyramidPass.
    struct DepthPyramidInfo
    {
        // Where the texture table has it, with all its sizes.
        uint32_t TextureIndex = 0;

        // The largest size, in pixels.
        uint32_t Width = 0;
        uint32_t Height = 0;

        // Zero if no pyramid was built in this frame.
        uint32_t MipCount = 0;
    };

    struct SceneData
    {
        glm::mat4 ViewProjectionMatrix;
        glm::mat4 LightViewProjection;
        glm::vec4 CameraPosition;
        glm::vec4 LightDirection; // w is intensity
        glm::vec4 LightColor;
    };

    // One see-through instance, drawn on its own: these are sorted by distance, which a shared draw
    // command cannot do. FirstInstance is its place in the frame's list of drawn instances.
    struct TransparentDraw
    {
        uint32_t IndexCount;
        uint32_t FirstIndex;
        int32_t VertexOffset;
        uint32_t FirstInstance;
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
        // In the order they are drawn, the farthest first. Valid only while the frame is drawn.
        std::span<const TransparentDraw> TransparentDraws;
        MeshDrawCommands MeshDraws;
        MeshCulling Culling;

        std::vector<ParticleBatch> ParticleQueue;

        // Holds the vertices and indices of every mesh. A pass that draws meshes binds it once.
        const GeometryPool* Geometry = nullptr;

        // The addresses shaders read the scene's mesh instances from, one GpuInstance each, and the list
        // of the places of those that this frame draws. A draw command covers a run of that list. See GpuScene.
        uint64_t MeshInstanceAddress = 0;
        uint64_t VisibleInstanceAddress = 0;

        // The address shaders read the material table from, see MaterialTable.
        uint64_t MaterialAddress = 0;

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

        // The two above as shaders get them. The engine's matrices have +Y up on screen, as GLM makes them.
        // Vulkan has +Y down, so what goes to a shader is turned around once, here.
        glm::mat4 ClipViewProjection;
        glm::mat4 ClipLightViewProjection;
        // The shadow map, and where the texture table and the table of comparing samplers have it and its sampler.
        GpuTexture* ShadowMap = nullptr;
        uint32_t ShadowMapIndex = 0;
        uint32_t ShadowSamplerIndex = 0;

        // Where the scene passes draw: the HDR picture, its depth and, in the editor, the entity ID of every pixel.
        GpuTexture* SceneColor = nullptr;
        GpuTexture* SceneDepth = nullptr;
        GpuTexture* SceneEntityIds = nullptr;

        // Where the finished picture goes: the window, or the editor's viewport picture.
        GpuTexture* Output = nullptr;

        // Where the texture table has the scene's depth.
        uint32_t SceneDepthIndex = 0;

        // Built from the depth of what the early phase drew, for the late phase of culling.
        DepthPyramidInfo DepthPyramid;

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
        GraphicsContext* Graphics = nullptr;
        UploadQueue* Uploads = nullptr;
        SharedBindings* Shared = nullptr;

        // The command buffer of the frame that is being recorded, as Vulkan's plain handle:
        // this header is included by the editor and stays free of Vulkan's own.
        VkCommandBuffer_T* Commands = nullptr;

        // True in the editor, where the scene has a second target that holds entity IDs for picking.
        bool WritesEntityIds = false;
    };


    // Rebuilds a pass's pipelines whenever its shader is loaded or hot-reloaded.
    class PipelineState
    {
    public:
        // The path is relative to the engine resource directory, e.g. "Shaders/Bloom.slang".
        void SetShader(const std::filesystem::path& engineShaderPath) { m_ShaderPath = engineShaderPath; m_Shader = nullptr; }
        bool Update(std::function<void(std::shared_ptr<Shader>)> builder);

    private:
        std::filesystem::path m_ShaderPath;
        std::shared_ptr<Shader> m_Shader;
        uint32_t m_Version = -1;
    };
}
