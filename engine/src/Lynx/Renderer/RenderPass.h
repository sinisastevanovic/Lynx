#pragma once
#include <glm/glm.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <span>

#include "Atmosphere.h"
#include "DebugRenderer.h"
#include "ShadowCascades.h"
#include "ViewMode.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Scene/Exposure.h"
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

    // What about a material decides how the mesh passes draw a surface: with which pipeline, and in which
    // state. The draw commands of a frame are grouped by it, so that a pass sets each state once and draws
    // a whole group with one call. A further way of drawing is a further member here and a larger Count,
    // not another set of ranges in every pass.
    struct MeshDrawKind
    {
        // Only a material with cut-outs needs its texture where nothing but depth is drawn.
        bool HasCutOuts = false;

        // Seen from both sides, so no triangle is left out for facing away.
        bool IsTwoSided = false;

        static constexpr uint32_t Count = 4;

        uint32_t Index() const { return (HasCutOuts ? 1u : 0u) | (IsTwoSided ? 2u : 0u); }
        static MeshDrawKind At(uint32_t index) { return { (index & 1u) != 0, (index & 2u) != 0 }; }
    };

    // The draw commands of one view: a run per kind. The runs follow each other directly, in the order
    // of the kinds, and a kind that nothing is drawn in has an empty run.
    struct MeshDrawGroups
    {
        std::array<DrawRange, MeshDrawKind::Count> Ranges;

        DrawRange Of(MeshDrawKind kind) const { return Ranges[kind.Index()]; }

        DrawRange All() const
        {
            DrawRange all{ Ranges.front().First, 0 };
            for (const DrawRange& range : Ranges)
                all.Count += range.Count;
            return all;
        }
    };

    // The draw commands for the frame's meshes that are not see-through: one command per submesh and all
    // its instances, in a buffer on the GPU. A pass draws a whole range of them with one call.
    // Culling on the GPU decides how many instances each command draws, which may be none.
    struct MeshDrawCommands
    {
        const GpuBuffer* Buffer = nullptr;

        // What the camera sees, early and late, then what the sun sees in each cascade of its shadow.
        // The camera's commands of both phases follow each other directly.
        MeshDrawGroups CameraEarly;
        MeshDrawGroups CameraLate;
        std::array<MeshDrawGroups, ShadowCascadeCount> Shadow;

        const MeshDrawGroups& CameraOf(CullPhase phase) const { return phase == CullPhase::Early ? CameraEarly : CameraLate; }

        // Everything the camera sees that is not see-through.
        DrawRange Camera() const { return { CameraEarly.All().First, CameraEarly.All().Count + CameraLate.All().Count }; }
    };

    // How the level of detail of a mesh is picked, see Cull.slang.
    struct LodSettings
    {
        // A coarser level is drawn once its surface is at most this far from the original's, in pixels on screen.
        float ErrorPixels = 1.0f;

        // Foliage is drawn with fewer and larger leaves once a leaf is at most this large on screen, in
        // pixels, and with half as many again every time the leaves have grown back to that size.
        float LeafPixels = 8.0f;

        // How many levels coarser than for the camera a mesh is drawn into the shadow map.
        uint32_t ShadowBias = 1;

        // Draws every mesh in this level, or in its coarsest if it has fewer: for looking at the levels.
        // Negative for the level its distance picks.
        int32_t ForcedLevel = -1;
    };

    // What culling on the GPU works with besides the instances, see GpuScene and CullPass.
    struct MeshCulling
    {
        // The frame's draw commands, into which culling counts the instances that are seen.
        uint64_t CommandAddress = 0;

        // Per batch of instances, where the list of levels has those of the batch.
        uint64_t BatchAddress = 0;

        // Per level of a batch, its error and the commands it is drawn with: the camera's early and late, and the sun's.
        uint64_t LodAddress = 0;

        // The planes of the camera's view and of the sun's, how the camera projects and how levels are picked.
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

    // What every shader knows about the frame. Has to match SceneConstants in Scene.slang.
    // How bright the picture is made, as it lives on the GPU. Has to match ExposureState in Scene.slang.
    struct ExposureState
    {
        float Exposure = 1.0f;
        uint32_t IsAdapted = 0;
    };

    struct SceneData
    {
        glm::mat4 ViewProjectionMatrix;

        // The same of the frame before.
        glm::mat4 PreviousViewProjectionMatrix;
        glm::vec4 CameraPosition;
        glm::vec4 LightDirection; // w is intensity
        glm::vec4 LightColor;

        // The sun's shadow. The world as the sun sees it, turned but not moved. Then per cascade: from the
        // world to where a place lies in the cascade's map (xy as texture coordinates, z as the depth to
        // compare), the square of ground the map covers as the sun sees it (its middle in xy, one over half
        // its width in z, and in w the size of a texel in the world), where the texture table has the map,
        // and how far the map reaches along the sun's light: its depths from 0 to 1 stand for that distance.
        glm::mat4 SunView;
        glm::mat4 CascadeToTexture[ShadowCascadeCount];
        glm::vec4 CascadeFootprint[ShadowCascadeCount];
        glm::uvec4 CascadeTextures;
        glm::vec4 CascadeDepth;

        // Where the table of comparing samplers has the one the maps are compared with, and the table of
        // samplers the one their depths are read with.
        uint32_t ShadowSampler;
        uint32_t ShadowDepthSampler;

        // The edge of a shadow gets this much wider, from its middle to its rim, per unit of distance
        // between what casts the shadow and what receives it. And the widest it gets, in the scene's units.
        float ShadowSoftening;
        float ShadowWidestEdge;

        // The air, which colours the sky and the sun's light.
        AtmosphereData Atmosphere;

        // The address of the light of the sky on surfaces: nine numbers per colour, a vec4 each,
        // which SkyLightPass writes.
        uint64_t SkyIrradiance;

        // Where the table of cubes has the cube shiny surfaces mirror the sky from, and the sampler
        // table the sampler it is read with.
        uint32_t SkyReflection;
        uint32_t SkyReflectionSampler;

        // The address of an ExposureState: how bright the picture is made, which ExposurePass keeps up to date.
        uint64_t Exposure;
        uint64_t Padding;
    };

    // One see-through instance, drawn on its own: these are sorted by distance, which a shared draw
    // command cannot do. FirstInstance is its place in the frame's list of drawn instances.
    struct TransparentDraw
    {
        uint32_t IndexCount;
        uint32_t FirstIndex;
        int32_t VertexOffset;
        uint32_t FirstInstance;
        bool IsTwoSided;
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

    class GpuBuffer;

    // The patches of terrain a frame draws, as TerrainRenderer picked them. A patch is one instance of
    // the same small grid of triangles. Patches of a kind lie together in the list and are one draw call:
    // whole patches first, then the four quarters, each with its own run of the grid's indices.
    struct TerrainDraws
    {
        struct Run
        {
            uint32_t FirstIndex = 0;
            uint32_t IndexCount = 0;
            uint32_t FirstPatch = 0;
            uint32_t PatchCount = 0;
        };
        std::array<Run, 5> Runs{};

        // The grid's indices, and the addresses shaders read the patches and the terrains from.
        const GpuBuffer* Indices = nullptr;
        uint64_t PatchAddress = 0;
        uint64_t TerrainAddress = 0;

        uint32_t PatchCount = 0;
        uint64_t TriangleCount = 0;
    };

    // The terrain of a frame for every view it is drawn from: the camera's, and the sun's of each cascade
    // of its shadow. All of them are drawn as finely as the camera's distance asks for, so the shadow
    // of the ground has the shape of the ground that is seen.
    struct TerrainViews
    {
        TerrainDraws Camera;
        std::array<TerrainDraws, ShadowCascadeCount> Sun;
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

        // Where every instance is in the world, one InstanceTransform each, and where it was in the last frame.
        uint64_t MeshTransformAddress = 0;
        uint64_t PreviousMeshTransformAddress = 0;

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

        // How large the sun is in the sky, as an angle in degrees.
        float SunAngularSize = 0.53f;


        // The camera's matrix as shaders get it. The engine's matrices have +Y up on screen, as GLM makes them.
        // Vulkan has +Y down, so what goes to a shader is turned around once, here.
        glm::mat4 ClipViewProjection;

        // The cascades of the sun's shadow: what each covers, its matrix as shaders get it, and its map.
        std::array<ShadowCascade, ShadowCascadeCount> Cascades;
        std::array<glm::mat4, ShadowCascadeCount> ClipCascadeViewProjection;
        std::array<GpuTexture*, ShadowCascadeCount> ShadowMaps{};

        // The air above the scene, and the tables the sky is read from, in the order of AtmosphereTable.
        AtmosphereSettings Atmosphere;
        std::array<GpuTexture*, static_cast<size_t>(AtmosphereTable::Count)> AtmosphereTables{};

        // True once the atmosphere pass has drawn the tables in this frame. The passes that work the
        // sky's light out from them wait for that: the passes get their shaders one by one while the
        // program starts, and a result worked out from tables nobody has drawn yet would be kept.
        bool HasSkyView = false;

        // The cube shiny surfaces mirror the sky from, see SkyReflectionPass.
        GpuTexture* SkyReflection = nullptr;

        // The terrain of the frame.
        TerrainViews Terrain;

        // How the brightness of the picture is chosen, and the time since the last frame in seconds,
        // which the automatic exposure follows.
        ExposureSettings Exposure;
        float DeltaTime = 0.0f;

        // Where the scene passes draw: the HDR picture, its depth and, in the editor, the entity ID of every pixel.
        GpuTexture* SceneColor = nullptr;
        GpuTexture* SceneDepth = nullptr;
        GpuTexture* SceneEntityIds = nullptr;

        // Next to the colour: how far every pixel moved on screen since the last frame, see RenderFormats.
        // Written by the passes that draw what covers a pixel wholly: meshes, terrain and sky.
        GpuTexture* SceneMotion = nullptr;

        // Where the finished picture goes: the window, or the editor's viewport picture.
        GpuTexture* Output = nullptr;

        // Where the texture table has the scene's depth.
        uint32_t SceneDepthIndex = 0;

        // Built from the depth of what the early phase drew, for the late phase of culling.
        DepthPyramidInfo DepthPyramid;

        // Which stretch of its view the camera found something in a few frames ago, see DepthRangePass.
        // Nothing while that is not known: in the first frames, without a perspective, facing only sky.
        std::optional<ViewDepthRange> SeenDepthRange;

        // Where the texture table has the scene's HDR picture, and the bloom to add to it.
        // The bloom is a black picture unless the bloom pass has run in this frame.
        uint32_t SceneColorIndex = 0;
        uint32_t BloomTextureIndex = 0;

        // Where the texture table has the picture of the scene's motion.
        uint32_t SceneMotionIndex = 0;

        BloomSettings Bloom;

        // How the view that is being drawn is shown.
        ViewSettings Viewing;
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
