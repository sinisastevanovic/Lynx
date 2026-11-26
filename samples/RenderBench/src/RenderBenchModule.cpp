#include <Lynx.h>
#include <Lynx/GameModule.h>
#include <Lynx/Renderer/DebugRenderer.h>
#include <Lynx/Renderer/GpuScene.h>

#include "BenchScenes.h"

#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>

class RenderBenchModule : public IGameModule
{
public:
    void RegisterScripts(GameTypeRegistry*) override {}
    void RegisterComponents(GameTypeRegistry*) override {}

    void OnStart() override
    {
        Engine& engine = Engine::Get();
        m_Settings = SettingsFor(engine);
        if (!m_Settings)
            return;

        // The benchmark file decides which scene is built and how much it holds, see the files in bench/.
        const BenchmarkSettings& settings = *m_Settings;
        const auto count = [&settings](const char* name) { return static_cast<uint32_t>(std::max(settings.Parameter(name, 0.0), 0.0)); };

        // How levels of detail are picked, for measuring what they gain and for looking at one level.
        // CullCounts=1 makes culling count before it writes, 0 never: by itself the size of the scene decides.
        if (const double counts = settings.Parameter("CullCounts", -1.0); counts >= 0.0)
            engine.GetRenderer().GetGpuScene().SetCountsFirst(counts > 0.0);

        LodSettings& lods = engine.GetRenderer().GetLodSettings();
        lods.ErrorPixels = static_cast<float>(settings.Parameter("LodErrorPixels", lods.ErrorPixels));
        lods.LeafPixels = static_cast<float>(settings.Parameter("LodLeafPixels", lods.LeafPixels));
        lods.ShadowBias = static_cast<uint32_t>(std::max(settings.Parameter("LodShadowBias", lods.ShadowBias), 0.0));
        lods.ForcedLevel = static_cast<int32_t>(settings.Parameter("LodForcedLevel", lods.ForcedLevel));

        ShadowSettings& shadows = engine.GetRenderer().GetShadowSettings();
        shadows.Distance = static_cast<float>(settings.Parameter("ShadowDistance", shadows.Distance));
        shadows.SmallestCasterInTexels = static_cast<float>(settings.Parameter("ShadowSmallestCaster", shadows.SmallestCasterInTexels));
        shadows.FitsToView = settings.Parameter("ShadowFit", shadows.FitsToView ? 1.0 : 0.0) > 0.0;

        TemporalSettings& temporal = engine.GetRenderer().GetTemporalSettings();
        temporal.Jitters = settings.Parameter("Jitter", temporal.Jitters ? 1.0 : 0.0) > 0.0;
        temporal.JitterLength = static_cast<uint32_t>(std::max(settings.Parameter("JitterLength", temporal.JitterLength), 1.0));
        temporal.Accumulates = settings.Parameter("Accumulate", temporal.Accumulates ? 1.0 : 0.0) > 0.0;
        temporal.Resolves = settings.Parameter("TAA", temporal.Resolves ? 1.0 : 0.0) > 0.0;
        temporal.HistoryTime = static_cast<float>(settings.Parameter("TaaHistoryTime", temporal.HistoryTime));
        temporal.BoxGamma = static_cast<float>(settings.Parameter("TaaBoxGamma", temporal.BoxGamma));
        temporal.Sharpness = static_cast<float>(settings.Parameter("TaaSharpness", temporal.Sharpness));
        temporal.DepthTolerance = static_cast<float>(settings.Parameter("TaaDepthTolerance", temporal.DepthTolerance));
        temporal.BetweenPixelsShare = static_cast<float>(settings.Parameter("TaaBetweenPixelsShare", temporal.BetweenPixelsShare));
        temporal.SteadyTextures = settings.Parameter("TaaSteadyTextures", temporal.SteadyTextures ? 1.0 : 0.0) > 0.0;
        temporal.StillBoxScale = static_cast<float>(settings.Parameter("TaaStillBoxScale", temporal.StillBoxScale));
        temporal.VariesNoise = settings.Parameter("TaaVariesNoise", temporal.VariesNoise ? 1.0 : 0.0) > 0.0;
        temporal.StillHistoryTime = static_cast<float>(settings.Parameter("TaaStillHistoryTime", temporal.StillHistoryTime));
        if (settings.Parameter("FXAA", 1.0) <= 0.0)
            engine.GetRenderer().SetFXAAEnabled(false);

        AmbientOcclusionSettings& occlusion = engine.GetRenderer().GetAmbientOcclusionSettings();
        occlusion.Enabled = settings.Parameter("AO", occlusion.Enabled ? 1.0 : 0.0) > 0.0;
        occlusion.Radius = static_cast<float>(settings.Parameter("AoRadius", occlusion.Radius));
        occlusion.Quality = static_cast<AmbientOcclusionQuality>(std::clamp(static_cast<int>(settings.Parameter("AoQuality", static_cast<double>(occlusion.Quality))), 0, 3));
        occlusion.SmoothingRounds = static_cast<uint32_t>(std::clamp(static_cast<int>(settings.Parameter("AoSmooth", occlusion.SmoothingRounds)), 0, 3));
        occlusion.HistoryTime = static_cast<float>(settings.Parameter("AoHistoryTime", occlusion.HistoryTime));

        // What the view shows, for pictures of every view mode. See ViewMode for the numbers.
        ViewSettings& viewing = engine.GetSceneRenderer().GetViewSettings();
        const double mode = std::clamp(settings.Parameter("ViewMode", 0.0), 0.0, static_cast<double>(ViewModeNames.size() - 1));
        viewing.Mode = static_cast<ViewMode>(static_cast<uint32_t>(mode));
        viewing.ShowCutOuts = settings.Parameter("ShowCutOuts", 1.0) > 0.0;

        Scene& scene = *engine.GetActiveScene();
        AssetManager& assetManager = engine.GetAssetManager();

        // Exposure=<number> shows the scene with that exposure in place of the automatic one, which
        // follows what is on screen: two pictures can then be compared pixel by pixel when what they
        // show differs a little.
        if (const double exposure = settings.Parameter("Exposure", -1.0); exposure >= 0.0)
        {
            ExposureSettings fixedExposure;
            fixedExposure.IsAutomatic = false;
            fixedExposure.Fixed = static_cast<float>(exposure);
            const auto exposures = scene.Reg().view<ExposureSettings>();
            if (exposures.begin() == exposures.end())
                scene.CreateEntity("Exposure").AddComponent<ExposureSettings>(fixedExposure);
            for (const auto entity : exposures)
                exposures.get<ExposureSettings>(entity) = fixedExposure;
        }

        // How high the sun stands, in degrees above the horizon, for pictures of a low sun and long
        // shadows. Its compass bearing stays as the scene has it.
        if (const double elevation = settings.Parameter("SunElevation", -1.0); elevation >= 0.0)
        {
            const auto suns = scene.Reg().view<TransformComponent, DirectionalLightComponent>();
            for (const auto entity : suns)
            {
                TransformComponent& transform = suns.get<TransformComponent>(entity);
                const glm::vec3 shining = glm::rotate(transform.GetRotation(), glm::vec3(0.0f, 0.0f, -1.0f));
                const glm::vec2 bearing = glm::length(glm::vec2(shining.x, shining.z)) > 0.0001f ? glm::normalize(glm::vec2(shining.x, shining.z)) : glm::vec2(1.0f, 0.0f);
                const float angle = glm::radians(static_cast<float>(elevation));
                const glm::vec3 wanted(bearing.x * std::cos(angle), -std::sin(angle), bearing.y * std::cos(angle));
                transform.SetRotation(glm::rotation(glm::vec3(0.0f, 0.0f, -1.0f), wanted));
            }
        }

        if (const uint32_t instances = count("Instances"))
            CreateInstanceGrid(scene, assetManager, instances);

        if (const uint32_t dense = count("Dense"))
            CreateDenseField(scene, assetManager, dense);

        // Scatter=1 draws the trees as scattered instances in place of entities, and ScatterLike=1 gives
        // the entities the turns and sizes a scattered instance can hold, so that both show the same.
        if (const uint32_t trees = count("Trees"))
        {
            const TreesAs as = count("Scatter") > 0 ? TreesAs::Scattered : count("ScatterLike") > 0 ? TreesAs::EntitiesAsScattered : TreesAs::Entities;
            CreateTrees(scene, assetManager, trees, as);
        }

        if (const uint32_t terrain = count("Terrain"))
            CreateTerrain(scene, assetManager, terrain, settings.Parameter("TerrainThroughFile", 0.0) > 0.0);

        if (const uint32_t balls = count("TerrainBalls"))
            m_TerrainPhysics.emplace(scene, balls);

        const uint32_t crowd = count("Crowd");
        const uint32_t blocksPerSide = count("BlocksPerSide");
        const bool hasMotionProps = count("MotionProps") > 0;
        if (crowd == 0 && blocksPerSide == 0 && !hasMotionProps)
            return;

        // The scenes hold the meshes they use, so the library is only needed while they are built.
        const ShapeLibrary shapes(assetManager, std::max(count("Materials"), 1u));
        if (crowd > 0)
            m_Crowd.emplace(scene, shapes, crowd, count("ReplacedPerFrame"));
        if (blocksPerSide > 0)
            CreateBlocks(scene, shapes, blocksPerSide, count("PropsPerBlock"));
        if (hasMotionProps)
            m_MotionProps.emplace(scene, shapes);
    }

    void OnUpdate(float deltaTime) override
    {
        Engine& engine = Engine::Get();
        if (m_Crowd)
            m_Crowd->Update(*engine.GetActiveScene(), deltaTime);
        if (m_MotionProps)
            m_MotionProps->Update(*engine.GetActiveScene(), deltaTime);
        if (m_TerrainPhysics)
            m_TerrainPhysics->Update(*engine.GetActiveScene(), deltaTime);

        // Debug shapes last one frame, so they are drawn again in every frame. See bench/features.json.
        if (!m_Settings || m_Settings->Parameter("DebugShapes", 0.0) <= 0.0)
            return;

        DebugRenderer::DrawBox(glm::vec3(-7.0f, 0.0f, 6.0f), glm::vec3(-5.0f, 2.0f, 8.0f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
        DebugRenderer::DrawSphere(glm::vec3(4.0f, 1.5f, 6.0f), 1.5f, glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
        DebugRenderer::DrawLine(glm::vec3(-9.0f, 0.1f, 9.0f), glm::vec3(9.0f, 4.0f, -4.0f), glm::vec4(0.3f, 0.5f, 1.0f, 1.0f));

        // The pass that draws a texture for the editor's texture viewer has no window to ask for it
        // here. Asking for it keeps it under the eyes of the validation layer. Its picture is not shown.
        if (const std::shared_ptr<Texture> texture = engine.GetAssetManager().GetDefaultTexture(); texture && texture->IsOnGpu())
        {
            TexturePreview preview;
            preview.TextureIndex = texture->GetTableIndex();
            preview.Width = 256;
            preview.Height = 256;
            preview.ShowsAlpha = true;
            engine.GetRenderer().ShowTexturePreview(1, preview);

            // A second one, as a second window would ask for it, of another size.
            preview.Width = 96;
            preview.ShowsAlpha = false;
            engine.GetRenderer().ShowTexturePreview(2, preview);
        }
    }

    void OnShutdown() override
    {
        m_Settings.reset();
        m_Crowd.reset();
        m_MotionProps.reset();
        m_TerrainPhysics.reset();
    }

private:
    // The numbers the scene that plays is built from. A benchmark that runs brings its own.
    //
    // In the editor no benchmark runs. Pressing Play there builds what the benchmark named like the
    // scene builds, Scenes/Crowd.lxscene from bench/crowd.json, so that a benchmark's scene can be
    // looked at with a free camera and the editor's settings. The benchmark's camera path, its
    // pictures and its measuring are left out.
    static std::optional<BenchmarkSettings> SettingsFor(Engine& engine)
    {
        if (const Benchmark* benchmark = engine.GetBenchmark())
            return benchmark->Settings();

        std::string name = std::filesystem::path(engine.GetActiveScene()->GetFilePath()).stem().string();
        std::ranges::transform(name, name.begin(), [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
        const std::filesystem::path file = engine.GetProject().RootDirectory / "bench" / (name + ".json");
        if (name.empty() || !std::filesystem::exists(file))
            return std::nullopt;

        auto settings = BenchmarkSettings::Load(file);
        if (!settings)
        {
            LX_ERROR("RenderBench: {0}", settings.error());
            return std::nullopt;
        }

        LX_INFO("RenderBench: building the scene of bench/{0}.json to look at.", name);
        return std::move(*settings);
    }

    std::optional<BenchmarkSettings> m_Settings;
    std::optional<Crowd> m_Crowd;
    std::optional<MotionProps> m_MotionProps;
    std::optional<TerrainPhysicsCheck> m_TerrainPhysics;
};

LX_GAME_MODULE(RenderBenchModule)
