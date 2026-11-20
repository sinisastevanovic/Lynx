#include <Lynx.h>
#include <Lynx/GameModule.h>

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

    void OnUpdate(float) override {}
    void OnShutdown() override {}
};

LX_GAME_MODULE(RenderBenchModule)
