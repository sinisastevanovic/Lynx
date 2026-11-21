#include "BenchScenes.h"

#include "Shapes.h"

#include <Lynx/Asset/Texture.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <iterator>

namespace
{
    // The top of the ground that the benchmark scenes are saved with.
    constexpr float GroundHeight = -0.5f;

    // Mixes the bits of a number so that neighbouring numbers give unrelated results.
    uint32_t Hash(uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    // A number from 0 up to, but not including, 1. Each stream gives a different one for the same index.
    float Random(uint32_t index, uint32_t stream)
    {
        return static_cast<float>(Hash(index * 16u + stream) >> 8) / 16777216.0f;
    }

    float Random(uint32_t index, uint32_t stream, float smallest, float largest)
    {
        return smallest + (largest - smallest) * Random(index, stream);
    }

    // A fully saturated colour for a position on the colour wheel, from 0 to 1.
    glm::vec3 ColourOfHue(float hue)
    {
        const glm::vec3 offsets(0.0f, 2.0f / 3.0f, 1.0f / 3.0f);
        return glm::clamp(glm::abs(glm::fract(hue + offsets) * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
    }

    glm::quat TurnedAroundUp(float turns)
    {
        return glm::angleAxis(turns * glm::two_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    Entity CreateMeshEntity(Scene& scene, const char* name, const std::shared_ptr<StaticMesh>& mesh, const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        Entity entity = scene.CreateEntity(name);
        entity.AddComponent<MeshComponent>().Mesh = AssetRef<StaticMesh>(mesh);

        TransformComponent& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = position;
        transform.Rotation = rotation;
        transform.Scale = scale;
        return entity;
    }

    // A small shape in a material and a size that depend only on the index.
    struct Prop
    {
        std::shared_ptr<StaticMesh> Mesh;
        float Size;
    };

    Prop ChooseProp(const ShapeLibrary& shapes, uint32_t index)
    {
        const uint32_t shape = Hash(index * 16u + 0u) % shapes.ShapeCount();
        const uint32_t material = Hash(index * 16u + 1u) % shapes.MaterialCount();
        return { shapes.Mesh(shape, material), Random(index, 2, 0.6f, 1.4f) };
    }
}

ShapeLibrary::ShapeLibrary(AssetManager& assetManager, uint32_t materialCount)
    : m_MaterialCount(materialCount)
{
    const ShapeGeometry geometries[] = {
        MakeBox(),
        MakeSphere(24, 12),
        MakeCylinder(24),
        MakeTorus(24, 12),
    };
    m_ShapeCount = static_cast<uint32_t>(std::size(geometries));

    // The materials share a few textures, as the materials of a real scene do.
    // Loaded here and now, so that they are on the GPU before the first frame is drawn.
    const char* const texturePaths[] = {
        "Models/Sponza/7268504077753552595.jpg",
        "Models/Sponza/8750083169368950601.jpg",
        "Models/Sponza/5792855332885324923.jpg",
        "Models/Sponza/14650633544276105767.jpg",
        "Models/Sponza/6047387724914829168.jpg",
        "Models/Sponza/5823059166183034438.jpg",
        "Models/Sponza/7441062115984513793.jpg",
        "Models/Sponza/11490520546946913238.jpg",
    };
    std::vector<AssetHandle> textures;
    for (const char* path : texturePaths)
    {
        if (const std::shared_ptr<Texture> texture = assetManager.GetProjectAsset<Texture>(path, AssetLoadMode::Blocking))
            textures.push_back(texture->GetHandle());
        else
            LX_ERROR("RenderBench: cannot load {0}", path);
    }

    // The renderer draws meshes in the order of their handles, so these are fixed.
    const uint64_t firstHandle = 7300000000000000000ull;

    m_Meshes.reserve(static_cast<size_t>(m_ShapeCount) * materialCount);
    for (uint32_t shape = 0; shape < m_ShapeCount; ++shape)
    {
        for (uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex)
        {
            auto mesh = std::make_shared<StaticMesh>(geometries[shape].Vertices, geometries[shape].Indices);
            assetManager.AddRuntimeAsset(mesh, AssetHandle(firstHandle + m_Meshes.size()));

            // A mesh made in code comes with a material of its own that shows the checker texture.
            // One material in every nine keeps it.
            Material& material = *mesh->GetSubmeshes()[0].Material;
            const float hue = static_cast<float>(materialIndex) / static_cast<float>(materialCount);
            material.AlbedoColor = glm::vec4(glm::mix(glm::vec3(1.0f), ColourOfHue(hue), 0.5f), 1.0f);
            material.Roughness = Random(materialIndex, 0, 0.25f, 0.85f);
            material.Metallic = materialIndex % 5 == 0 ? 1.0f : 0.0f;

            const size_t textureChoice = materialIndex % (textures.size() + 1);
            if (textureChoice < textures.size())
                material.AlbedoTexture = textures[textureChoice];

            m_Meshes.push_back(std::move(mesh));
        }
    }

    LX_INFO("Created {0} meshes: {1} shapes in {2} materials.", m_Meshes.size(), m_ShapeCount, materialCount);
}

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

Crowd::Crowd(Scene& scene, const ShapeLibrary& shapes, uint32_t count)
{
    const float spacing = 2.5f;
    const uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float origin = -0.5f * spacing * static_cast<float>(side - 1);

    for (uint32_t index = 0; index < count; ++index)
    {
        const Prop prop = ChooseProp(shapes, index);

        // Each one stands a little off its place in the rows, resting on the ground.
        const glm::vec3 position(
            origin + spacing * static_cast<float>(index % side) + Random(index, 3, -0.6f, 0.6f),
            GroundHeight + 0.5f * prop.Size,
            origin + spacing * static_cast<float>(index / side) + Random(index, 4, -0.6f, 0.6f));

        const Entity entity = CreateMeshEntity(scene, "Instance", prop.Mesh, position, TurnedAroundUp(Random(index, 5)), glm::vec3(prop.Size));

        if (index % 5 == 0)
            m_Movers.push_back({ entity, position, Random(index, 6, 0.5f, 1.5f), Random(index, 7, 0.5f, 2.0f), Random(index, 8, 0.0f, glm::two_pi<float>()) });
    }

    LX_INFO("Created a crowd of {0} instances, {1} of them moving.", count, m_Movers.size());
}

void Crowd::Update(Scene& scene, float deltaTime)
{
    // A benchmark advances every frame by the same time, so frame N shows the same poses in every run.
    m_Time += deltaTime;

    auto& registry = scene.Reg();
    for (const Mover& mover : m_Movers)
    {
        const float angle = mover.Phase + mover.Speed * m_Time;
        const glm::vec3 offset(std::cos(angle) * mover.Radius, 0.5f + 0.5f * std::sin(2.0f * angle), std::sin(angle) * mover.Radius);
        registry.get<TransformComponent>(mover.Handle).Translation = mover.Home + offset;
    }
}

void CreateBlocks(Scene& scene, const ShapeLibrary& shapes, uint32_t blocksPerSide, uint32_t propsPerBlock)
{
    // A block and half a street on each of its sides make one cell of the city.
    const float cellSize = 24.0f;
    const float blockSize = 16.0f;
    const float origin = -0.5f * cellSize * static_cast<float>(blocksPerSide - 1);

    // The streets are the part of a cell outside its block. Props keep a little away from the walls.
    const float wallDistance = 0.5f * blockSize;
    const float nearestToWall = wallDistance + 0.5f;
    const float farthestFromWall = 0.5f * cellSize - 0.5f;

    uint32_t propIndex = 0;
    for (uint32_t blockIndex = 0; blockIndex < blocksPerSide * blocksPerSide; ++blockIndex)
    {
        const glm::vec3 centre(
            origin + cellSize * static_cast<float>(blockIndex % blocksPerSide),
            GroundHeight,
            origin + cellSize * static_cast<float>(blockIndex / blocksPerSide));

        const float height = Random(blockIndex, 9, 20.0f, 60.0f);
        const uint32_t material = Hash(blockIndex * 16u + 10u) % shapes.MaterialCount();
        CreateMeshEntity(scene, "Block", shapes.Mesh(ShapeLibrary::BoxShape, material),
            centre + glm::vec3(0.0f, 0.5f * height, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(blockSize, height, blockSize));

        for (uint32_t prop = 0; prop < propsPerBlock; ++prop, ++propIndex)
        {
            const Prop chosen = ChooseProp(shapes, propIndex);

            // Somewhere along one of the four streets around the block.
            const float along = Random(propIndex, 3, -0.5f * cellSize, 0.5f * cellSize);
            const float away = Random(propIndex, 4, nearestToWall, farthestFromWall);
            const uint32_t street = Hash(propIndex * 16u + 5u) % 4;
            const glm::vec3 offset = street == 0 ? glm::vec3(along, 0.0f, away)
                : street == 1 ? glm::vec3(along, 0.0f, -away)
                : street == 2 ? glm::vec3(away, 0.0f, along)
                : glm::vec3(-away, 0.0f, along);

            CreateMeshEntity(scene, "Prop", chosen.Mesh, centre + offset + glm::vec3(0.0f, 0.5f * chosen.Size, 0.0f),
                TurnedAroundUp(Random(propIndex, 6)), glm::vec3(chosen.Size));
        }
    }

    LX_INFO("Created {0} blocks with {1} props between them.", blocksPerSide * blocksPerSide, propIndex);
}

void CreateDenseField(Scene& scene, AssetManager& assetManager, uint32_t count)
{
    // The scale brings every model to a few metres, so that none of them fills the view alone.
    struct Kind
    {
        const char* Path;
        float Scale;
    };

    const Kind kinds[] = {
        { "Models/coast_rocks_05/coast_rocks_05_2k.gltf", 1.0f },
        { "Models/root_cluster_01/root_cluster_01_2k.gltf", 1.0f },
        { "Models/potted_plant_01/potted_plant_01_2k.gltf", 2.5f },
        { "Models/shrub_01/shrub_01_2k.gltf", 1.5f },
        { "Models/dutch_ship_large_01/dutch_ship_large_01_2k.gltf", 0.2f },
        { "Models/FlightHelmet/FlightHelmet.gltf", 5.0f },
    };

    std::vector<std::shared_ptr<StaticMesh>> meshes;
    for (const Kind& kind : kinds)
    {
        meshes.push_back(assetManager.GetProjectAsset<StaticMesh>(kind.Path, AssetLoadMode::Blocking));
        if (!meshes.back())
            LX_ERROR("RenderBench: cannot load {0}", kind.Path);
    }

    const float spacing = 8.0f;
    const uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float origin = -0.5f * spacing * static_cast<float>(side - 1);

    for (uint32_t index = 0; index < count; ++index)
    {
        // Mixed by a hash, so that no row or column holds one kind only.
        const size_t kind = Hash(index * 16u + 0u) % std::size(kinds);
        if (!meshes[kind])
            continue;

        const glm::vec3 position(
            origin + spacing * static_cast<float>(index % side) + Random(index, 3, -1.0f, 1.0f),
            GroundHeight,
            origin + spacing * static_cast<float>(index / side) + Random(index, 4, -1.0f, 1.0f));

        CreateMeshEntity(scene, "Model", meshes[kind], position, TurnedAroundUp(Random(index, 5)), glm::vec3(kinds[kind].Scale));
    }

    LX_INFO("Created a field of {0} detailed models.", count);
}
