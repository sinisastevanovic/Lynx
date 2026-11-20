#include <Lynx.h>
#include <Lynx/GameModule.h>
#include <Lynx/Renderer/DebugRenderer.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace
{
    // Fills a square field with copies of a few meshes. The layout depends only on the count,
    // so every run of a benchmark draws the same scene.
    void CreateInstanceGrid(Scene& scene, AssetManager& assetManager, uint32_t count)
    {
        struct Kind
        {
            std::shared_ptr<StaticMesh> Mesh;
            float Scale;
            float Height;
        };

        const Kind kinds[] = {
            { assetManager.GetDefaultCube(), 1.0f, 0.0f },
            { assetManager.GetEngineAsset<StaticMesh>("SM_DefaultSphere.gltf", AssetLoadMode::Blocking), 1.0f, 0.0f },
            { assetManager.GetProjectAsset<StaticMesh>("Models/Bottle/WaterBottle.gltf", AssetLoadMode::Blocking), 5.0f, 0.15f },
        };

        const float spacing = 2.0f;
        const uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
        const float origin = -0.5f * spacing * static_cast<float>(side - 1);

        for (uint32_t index = 0; index < count; ++index)
        {
            const uint32_t column = index % side;
            const uint32_t row = index / side;
            const Kind& kind = kinds[(column + row * 2) % std::size(kinds)];

            Entity entity = scene.CreateEntity("Instance");
            entity.AddComponent<MeshComponent>().Mesh = AssetRef<StaticMesh>(kind.Mesh);

            TransformComponent& transform = entity.GetComponent<TransformComponent>();
            transform.Translation = { origin + spacing * static_cast<float>(column), kind.Height, origin + spacing * static_cast<float>(row) };
            transform.Rotation = glm::angleAxis(glm::radians(static_cast<float>((index * 37) % 360)), glm::vec3(0.0f, 1.0f, 0.0f));
            transform.Scale = glm::vec3(kind.Scale);
        }

        LX_INFO("Created a grid of {0} instances.", count);
    }
}

class RenderBenchModule : public IGameModule
{
public:
    void RegisterScripts(GameTypeRegistry*) override {}
    void RegisterComponents(GameTypeRegistry*) override {}

    void OnStart() override
    {
        Engine& engine = Engine::Get();

        // The benchmark file decides how much the scene holds, see bench/grid.json.
        const Benchmark* benchmark = engine.GetBenchmark();
        const double instances = benchmark ? benchmark->Settings().Parameter("Instances", 0.0) : 0.0;
        if (instances > 0.0)
            CreateInstanceGrid(*engine.GetActiveScene(), engine.GetAssetManager(), static_cast<uint32_t>(instances));
    }

    void OnUpdate(float) override
    {
        // Debug shapes last one frame, so they are drawn again in every frame. See bench/features.json.
        const Benchmark* benchmark = Engine::Get().GetBenchmark();
        if (!benchmark || benchmark->Settings().Parameter("DebugShapes", 0.0) <= 0.0)
            return;

        DebugRenderer::DrawBox(glm::vec3(-7.0f, 0.0f, 6.0f), glm::vec3(-5.0f, 2.0f, 8.0f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
        DebugRenderer::DrawSphere(glm::vec3(4.0f, 1.5f, 6.0f), 1.5f, glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
        DebugRenderer::DrawLine(glm::vec3(-9.0f, 0.1f, 9.0f), glm::vec3(9.0f, 4.0f, -4.0f), glm::vec4(0.3f, 0.5f, 1.0f, 1.0f));
    }
    void OnShutdown() override {}
};

LX_GAME_MODULE(RenderBenchModule)
