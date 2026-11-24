#include <Lynx.h>
#include <Lynx/GameModule.h>
#include <Lynx/Renderer/DebugRenderer.h>

#include "BenchScenes.h"

#include <optional>

class RenderBenchModule : public IGameModule
{
public:
    void RegisterScripts(GameTypeRegistry*) override {}
    void RegisterComponents(GameTypeRegistry*) override {}

    void OnStart() override
    {
        Engine& engine = Engine::Get();
        const Benchmark* benchmark = engine.GetBenchmark();
        if (!benchmark)
            return;

        // The benchmark file decides which scene is built and how much it holds, see the files in bench/.
        const BenchmarkSettings& settings = benchmark->Settings();
        const auto count = [&settings](const char* name) { return static_cast<uint32_t>(std::max(settings.Parameter(name, 0.0), 0.0)); };

        // How levels of detail are picked, for measuring what they gain and for looking at one level.
        LodSettings& lods = engine.GetRenderer().GetLodSettings();
        lods.ErrorPixels = static_cast<float>(settings.Parameter("LodErrorPixels", lods.ErrorPixels));
        lods.LeafPixels = static_cast<float>(settings.Parameter("LodLeafPixels", lods.LeafPixels));
        lods.ShadowBias = static_cast<uint32_t>(std::max(settings.Parameter("LodShadowBias", lods.ShadowBias), 0.0));
        lods.ForcedLevel = static_cast<int32_t>(settings.Parameter("LodForcedLevel", lods.ForcedLevel));

        // What the view shows, for pictures of every view mode. See ViewMode for the numbers.
        ViewSettings& viewing = engine.GetSceneRenderer().GetViewSettings();
        const double mode = std::clamp(settings.Parameter("ViewMode", 0.0), 0.0, static_cast<double>(ViewModeNames.size() - 1));
        viewing.Mode = static_cast<ViewMode>(static_cast<uint32_t>(mode));
        viewing.ShowCutOuts = settings.Parameter("ShowCutOuts", 1.0) > 0.0;

        Scene& scene = *engine.GetActiveScene();
        AssetManager& assetManager = engine.GetAssetManager();

        if (const uint32_t instances = count("Instances"))
            CreateInstanceGrid(scene, assetManager, instances);

        if (const uint32_t dense = count("Dense"))
            CreateDenseField(scene, assetManager, dense);

        if (const uint32_t trees = count("Trees"))
            CreateTrees(scene, assetManager, trees);

        const uint32_t crowd = count("Crowd");
        const uint32_t blocksPerSide = count("BlocksPerSide");
        if (crowd == 0 && blocksPerSide == 0)
            return;

        // The scenes hold the meshes they use, so the library is only needed while they are built.
        const ShapeLibrary shapes(assetManager, std::max(count("Materials"), 1u));
        if (crowd > 0)
            m_Crowd.emplace(scene, shapes, crowd, count("ReplacedPerFrame"));
        if (blocksPerSide > 0)
            CreateBlocks(scene, shapes, blocksPerSide, count("PropsPerBlock"));
    }

    void OnUpdate(float deltaTime) override
    {
        Engine& engine = Engine::Get();
        if (m_Crowd)
            m_Crowd->Update(*engine.GetActiveScene(), deltaTime);

        // Debug shapes last one frame, so they are drawn again in every frame. See bench/features.json.
        const Benchmark* benchmark = engine.GetBenchmark();
        if (!benchmark || benchmark->Settings().Parameter("DebugShapes", 0.0) <= 0.0)
            return;

        DebugRenderer::DrawBox(glm::vec3(-7.0f, 0.0f, 6.0f), glm::vec3(-5.0f, 2.0f, 8.0f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
        DebugRenderer::DrawSphere(glm::vec3(4.0f, 1.5f, 6.0f), 1.5f, glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
        DebugRenderer::DrawLine(glm::vec3(-9.0f, 0.1f, 9.0f), glm::vec3(9.0f, 4.0f, -4.0f), glm::vec4(0.3f, 0.5f, 1.0f, 1.0f));
    }

    void OnShutdown() override
    {
        m_Crowd.reset();
    }

private:
    std::optional<Crowd> m_Crowd;
};

LX_GAME_MODULE(RenderBenchModule)
