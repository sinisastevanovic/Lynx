#include "BenchScenes.h"

#include "Shapes.h"

#include <Lynx/Asset/Material.h>
#include <Lynx/Asset/Terrain.h>
#include <Lynx/Asset/Texture.h>
#include <Lynx/Scene/Components/TerrainComponent.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <string>

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

    Entity CreateMeshEntity(Scene& scene, const char* name, const std::shared_ptr<StaticMesh>& mesh, const std::shared_ptr<Material>& material, const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        Entity entity = scene.CreateEntity(name);
        MeshComponent& meshComponent = entity.AddComponent<MeshComponent>();
        meshComponent.SetMesh(mesh->GetHandle());

        // Without a material the mesh keeps the ones it came with.
        if (material)
            meshComponent.SetMaterial(0, material->GetHandle());

        entity.GetComponent<TransformComponent>().SetTransform(position, rotation, scale);
        return entity;
    }

    // A small shape in a material and a size that depend only on the index.
    struct Prop
    {
        std::shared_ptr<StaticMesh> Mesh;
        std::shared_ptr<Material> Material;
        float Size;
    };

    Prop ChooseProp(const ShapeLibrary& shapes, uint32_t index)
    {
        const uint32_t shape = Hash(index * 16u + 0u) % shapes.ShapeCount();
        const uint32_t material = Hash(index * 16u + 1u) % shapes.MaterialCount();
        return { shapes.Mesh(shape), shapes.MaterialAt(material), Random(index, 2, 0.6f, 1.4f) };
    }
}

ShapeLibrary::ShapeLibrary(AssetManager& assetManager, uint32_t materialCount)
{
    const ShapeGeometry geometries[] = {
        MakeBox(),
        MakeSphere(24, 12),
        MakeCylinder(24),
        MakeTorus(24, 12),
    };

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

    // The renderer draws in the order of the handles of meshes and materials, so these are fixed.
    const uint64_t firstMeshHandle = 7300000000000000000ull;
    const uint64_t firstMaterialHandle = 7400000000000000000ull;

    for (const ShapeGeometry& geometry : geometries)
    {
        auto mesh = std::make_shared<StaticMesh>(geometry.Vertices, geometry.Indices);
        assetManager.AddRuntimeAsset(mesh, AssetHandle(firstMeshHandle + m_Meshes.size()));
        m_Meshes.push_back(std::move(mesh));
    }

    // One material in every nine shows the checker texture instead of one of Sponza's.
    const std::shared_ptr<Texture> checker = assetManager.GetDefaultTexture();
    for (uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex)
    {
        auto material = std::make_shared<Material>();
        const float hue = static_cast<float>(materialIndex) / static_cast<float>(materialCount);
        material->AlbedoColor = glm::vec4(glm::mix(glm::vec3(1.0f), ColourOfHue(hue), 0.5f), 1.0f);
        material->Roughness = Random(materialIndex, 0, 0.25f, 0.85f);
        material->Metallic = materialIndex % 5 == 0 ? 1.0f : 0.0f;

        const size_t textureChoice = materialIndex % (textures.size() + 1);
        if (textureChoice < textures.size())
            material->AlbedoTexture = textures[textureChoice];
        else if (checker)
            material->AlbedoTexture = checker->GetHandle();

        assetManager.AddRuntimeAsset(material, AssetHandle(firstMaterialHandle + materialIndex));
        m_Materials.push_back(std::move(material));
    }

    LX_INFO("Created {0} shapes and {1} materials.", m_Meshes.size(), m_Materials.size());
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
        entity.AddComponent<MeshComponent>().SetMesh(kind.Mesh->GetHandle());

        entity.GetComponent<TransformComponent>().SetTransform(
            { origin + spacing * static_cast<float>(column), kind.Height, origin + spacing * static_cast<float>(row) },
            glm::angleAxis(glm::radians(static_cast<float>((index * 37) % 360)), glm::vec3(0.0f, 1.0f, 0.0f)),
            glm::vec3(kind.Scale));
    }

    LX_INFO("Created a grid of {0} instances.", count);
}

Crowd::Crowd(Scene& scene, const ShapeLibrary& shapes, uint32_t count, uint32_t replacedPerFrame)
    : m_Shapes(shapes), m_Side(static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))))), m_ReplacedPerFrame(replacedPerFrame), m_NextPropNumber(count)
{
    for (uint32_t index = 0; index < count; ++index)
    {
        glm::vec3 position;
        const Entity entity = Place(scene, index, index, position);

        if (index % 5 == 0)
            m_Movers.push_back({ entity, position, Random(index, 6, 0.5f, 1.5f), Random(index, 7, 0.5f, 2.0f), Random(index, 8, 0.0f, glm::two_pi<float>()) });
        else
            m_Standing.push_back({ entity, index });
    }

    LX_INFO("Created a crowd of {0} instances, {1} of them moving, {2} replaced per frame.", count, m_Movers.size(), m_ReplacedPerFrame);
}

Entity Crowd::Place(Scene& scene, uint32_t cell, uint32_t propNumber, glm::vec3& position) const
{
    const float spacing = 2.5f;
    const float origin = -0.5f * spacing * static_cast<float>(m_Side - 1);
    const Prop prop = ChooseProp(m_Shapes, propNumber);

    // Each one stands a little off its place in the rows, resting on the ground.
    position = glm::vec3(
        origin + spacing * static_cast<float>(cell % m_Side) + Random(propNumber, 3, -0.6f, 0.6f),
        GroundHeight + 0.5f * prop.Size,
        origin + spacing * static_cast<float>(cell / m_Side) + Random(propNumber, 4, -0.6f, 0.6f));

    return CreateMeshEntity(scene, "Instance", prop.Mesh, prop.Material, position, TurnedAroundUp(Random(propNumber, 5)), glm::vec3(prop.Size));
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
        registry.get<TransformComponent>(mover.Handle).SetTranslation(mover.Home + offset);
    }

    // Time stands still while a benchmark warms up. Nothing is replaced then, so that the frames that
    // are measured replace the same props in every run.
    if (deltaTime <= 0.0f || m_Standing.empty())
        return;

    for (uint32_t count = 0; count < m_ReplacedPerFrame; ++count)
    {
        Standing& standing = m_Standing[Hash(m_NextPropNumber * 16u + 9u) % m_Standing.size()];
        scene.DestroyEntity(standing.Handle);

        glm::vec3 position;
        standing.Handle = Place(scene, standing.Cell, m_NextPropNumber++, position);
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
        CreateMeshEntity(scene, "Block", shapes.Mesh(ShapeLibrary::BoxShape), shapes.MaterialAt(material),
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

            CreateMeshEntity(scene, "Prop", chosen.Mesh, chosen.Material, centre + offset + glm::vec3(0.0f, 0.5f * chosen.Size, 0.0f),
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

        CreateMeshEntity(scene, "Model", meshes[kind], nullptr, position, TurnedAroundUp(Random(index, 5)), glm::vec3(kinds[kind].Scale));
    }

    LX_INFO("Created a field of {0} detailed models.", count);
}

void CreateTrees(Scene& scene, AssetManager& assetManager, uint32_t count)
{
    const char* const paths[] = {
        "Models/fir_sapling_medium/fir_sapling_medium_a.gltf",
        "Models/fir_sapling_medium/fir_sapling_medium_b.gltf",
        "Models/fir_sapling_medium/fir_sapling_medium_c.gltf",
        "Models/fir_tree_01/fir_tree_01_a.gltf",
        "Models/fir_tree_01/fir_tree_01_b.gltf",
        "Models/fir_tree_01/fir_tree_01_c.gltf",
        "Models/pine_tree_01/pine_tree_01_a.gltf",
        "Models/pine_tree_01/pine_tree_01_b.gltf",
        "Models/pine_tree_01/pine_tree_01_c.gltf",
    };

    std::vector<std::shared_ptr<StaticMesh>> meshes;
    for (const char* path : paths)
    {
        meshes.push_back(assetManager.GetProjectAsset<StaticMesh>(path, AssetLoadMode::Blocking));
        if (!meshes.back())
            LX_ERROR("RenderBench: cannot load {0}. The tree models are fetched by tools/fetch-polyhaven.ps1.", path);
    }

    const float spacing = 7.0f;
    const uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float origin = -0.5f * spacing * static_cast<float>(side - 1);

    for (uint32_t index = 0; index < count; ++index)
    {
        const size_t kind = Hash(index * 16u + 0u) % std::size(paths);
        if (!meshes[kind])
            continue;

        // Off the grid by up to a third of the spacing, so that no rows show.
        const glm::vec3 position(
            origin + spacing * static_cast<float>(index % side) + Random(index, 3, -2.3f, 2.3f),
            GroundHeight,
            origin + spacing * static_cast<float>(index / side) + Random(index, 4, -2.3f, 2.3f));

        CreateMeshEntity(scene, "Tree", meshes[kind], nullptr, position, TurnedAroundUp(Random(index, 5)), glm::vec3(Random(index, 6, 0.85f, 1.15f)));
    }

    LX_INFO("Created a stand of {0} trees.", count);
}

namespace
{
    // A number from 0 to 1 for a place on a grid, the same in every run.
    float NoiseAtCorner(int32_t x, int32_t z, uint32_t layer)
    {
        return static_cast<float>(Hash(static_cast<uint32_t>(x) * 0x9E3779B1u ^ Hash(static_cast<uint32_t>(z) + layer * 0x85EBCA6Bu)) >> 8) / 16777216.0f;
    }

    // The numbers of the grid, smoothly blended between its corners.
    float SmoothNoise(float x, float z, uint32_t layer)
    {
        const float cellX = std::floor(x);
        const float cellZ = std::floor(z);
        const auto eased = [](float t) { return t * t * (3.0f - 2.0f * t); };
        const float alongX = eased(x - cellX);
        const float alongZ = eased(z - cellZ);
        const int32_t ix = static_cast<int32_t>(cellX);
        const int32_t iz = static_cast<int32_t>(cellZ);
        return glm::mix(
            glm::mix(NoiseAtCorner(ix, iz, layer), NoiseAtCorner(ix + 1, iz, layer), alongX),
            glm::mix(NoiseAtCorner(ix, iz + 1, layer), NoiseAtCorner(ix + 1, iz + 1, layer), alongX),
            alongZ);
    }
}

void CreateTerrain(Scene& scene, AssetManager& assetManager, uint32_t resolution, bool throughFile)
{
    // Hills from seven layers of noise, each twice as fine and half as high as the one before.
    const float highest = 0.12f * static_cast<float>(resolution);
    std::vector<uint16_t> heights(static_cast<size_t>(resolution) * resolution);
    for (uint32_t z = 0; z < resolution; ++z)
    {
        for (uint32_t x = 0; x < resolution; ++x)
        {
            float height = 0.0f;
            float share = 0.5f;
            float cells = 4.0f / static_cast<float>(resolution);
            for (uint32_t layer = 0; layer < 7; ++layer)
            {
                height += share * SmoothNoise(static_cast<float>(x) * cells, static_cast<float>(z) * cells, layer);
                share *= 0.5f;
                cells *= 2.0f;
            }
            heights[static_cast<size_t>(z) * resolution + x] = static_cast<uint16_t>(std::clamp(height, 0.0f, 1.0f) * 65535.0f);
        }
    }

    // Three kinds of ground: forest floor where it is flat and low, rock where it is steep, mud high up.
    // One byte per layer and place, at half the resolution of the heights.
    const uint32_t weightResolution = (resolution - 1) / 2;
    std::vector<uint8_t> weights(static_cast<size_t>(weightResolution) * weightResolution * TerrainLayerCount, 0);
    const auto heightAt = [&](uint32_t x, uint32_t z)
    {
        return highest * static_cast<float>(heights[static_cast<size_t>(std::min(z, resolution - 1)) * resolution + std::min(x, resolution - 1)]) / 65535.0f;
    };
    for (uint32_t z = 0; z < weightResolution; ++z)
    {
        for (uint32_t x = 0; x < weightResolution; ++x)
        {
            // The rise over two steps of the heights, each way.
            const uint32_t hx = x * 2 + 1;
            const uint32_t hz = z * 2 + 1;
            const float riseX = (heightAt(hx + 1, hz) - heightAt(hx - 1, hz)) * 0.5f;
            const float riseZ = (heightAt(hx, hz + 1) - heightAt(hx, hz - 1)) * 0.5f;
            const float steepness = std::sqrt(riseX * riseX + riseZ * riseZ);

            const float rock = glm::smoothstep(0.35f, 0.6f, steepness);
            const float earth = (1.0f - rock) * glm::smoothstep(0.55f, 0.75f, heightAt(hx, hz) / highest);
            const float grass = 1.0f - rock - earth;

            uint8_t* place = &weights[(static_cast<size_t>(z) * weightResolution + x) * TerrainLayerCount];
            place[0] = static_cast<uint8_t>(grass * 255.0f);
            place[1] = static_cast<uint8_t>(rock * 255.0f);
            place[2] = static_cast<uint8_t>(earth * 255.0f);
        }
    }

    // The ground of a forest, from Poly Haven: its floor, rock, and mud with leaves. Each is three pictures,
    // the colour, the normal map, and one with roughness in green and metalness in blue.
    const auto makeLayer = [&assetManager](const char* name, float tileSize, uint64_t handle)
    {
        const auto load = [&](const char* map) -> AssetHandle
        {
            const std::string path = std::string("Textures/") + name + "/" + name + "_" + map + "_2k.jpg";
            if (const std::shared_ptr<Texture> texture = assetManager.GetProjectAsset<Texture>(path, AssetLoadMode::Blocking))
                return texture->GetHandle();

            LX_ERROR("RenderBench: cannot load {0}. The ground textures are fetched by tools/fetch-polyhaven.ps1.", path);
            return AssetHandle::Null();
        };

        // The pictures hold the whole look, so the material's own numbers leave them as they are.
        auto material = std::make_shared<Material>();
        material->AlbedoColor = glm::vec4(1.0f);
        material->Roughness = 1.0f;
        material->Metallic = 1.0f;
        material->AlbedoTexture = load("diff");
        material->NormalMap = load("nor_gl");
        material->MetallicRoughnessTexture = load("arm");
        material->UseNormalMap = true;
        assetManager.AddRuntimeAsset(material, AssetHandle(handle));
        return TerrainLayer{ material, tileSize };
    };
    const TerrainLayer layers[] = {
        makeLayer("forest_ground_04", 4.0f, 7600000000000000002ull),
        makeLayer("rock_face_03", 8.0f, 7600000000000000003ull),
        makeLayer("brown_mud_leaves_01", 4.0f, 7600000000000000004ull),
    };

    // One unit between heights. The handles are fixed, like those of the other things made here.
    const float size = static_cast<float>(resolution - 1);
    auto terrain = std::make_shared<Terrain>(resolution, size, 0.0f, highest, heights);
    terrain->SetLayers(layers, weightResolution, weights);

    // For checking the terrain's file: saved, and the one that is drawn read back from what was saved.
    // The pictures have to come out as without this.
    if (throughFile)
    {
        const std::filesystem::path file = Engine::Get().GetContentPaths().TempDirectory() / "BenchTerrain.lxterrain";
        if (terrain->Save(file))
        {
            if (std::shared_ptr<Terrain> loaded = Terrain::LoadFromFile(file))
                terrain = std::move(loaded);
            else
                LX_ERROR("RenderBench: cannot load the terrain back from {0}", file.string());
        }
    }
    assetManager.AddRuntimeAsset(terrain, AssetHandle(7600000000000000001ull));

    Entity entity = scene.CreateEntity("Terrain");
    entity.AddComponent<TerrainComponent>().Terrain = AssetRef<Terrain>(terrain);
    entity.GetComponent<TransformComponent>().SetTranslation(glm::vec3(-0.5f * size, 0.0f, -0.5f * size));

    LX_INFO("Created a terrain of {0} by {0} heights.", resolution);
}
