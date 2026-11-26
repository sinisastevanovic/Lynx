#include "Terrain.h"
#include "StaticMesh.h"

#include "Material.h"
#include "TextureSpecification.h"
#include "Serialization/TerrainSerializer.h"
#include "Lynx/Engine.h"
#include "Lynx/Renderer/Renderer.h"
#include "Lynx/Renderer/TextureSlot.h"
#include "Lynx/Renderer/Vulkan/GpuTexture.h"

#include <algorithm>
#include <bit>

namespace Lynx
{
    namespace
    {
        constexpr size_t BytesPerWeight = TerrainLayerCount;

        // A tile's picture holds one row more on every side than the tile itself. This is where a place
        // of a tile's picture lies in the whole: one before the tile's first place for the first row,
        // and never outside the whole, whose own edge is repeated instead.
        uint32_t InWhole(uint32_t tile, uint32_t perTile, uint32_t inPicture, uint32_t wholeCount)
        {
            const int64_t place = static_cast<int64_t>(tile) * perTile + inPicture - TerrainTileBorder;
            return static_cast<uint32_t>(std::clamp<int64_t>(place, 0, static_cast<int64_t>(wholeCount) - 1));
        }
    }

    Terrain::Terrain(uint32_t resolution, float size, float lowest, float highest, std::span<const uint16_t> heights, uint32_t tileQuads)
        : m_Size(size), m_Lowest(lowest), m_Highest(highest)
    {
        if (heights.size() < static_cast<size_t>(resolution) * resolution || !SetTiling(resolution, tileQuads))
        {
            LX_CORE_ERROR("A terrain needs at least {0} heights a side, and one for every place. It stays empty.", TerrainQuadsPerPatch + 1);
            SetState(AssetState::Error);
            return;
        }

        // The part of the heights the tiles cover: what lies beyond the last tile is left out.
        const uint32_t covered = GetResolution();
        if (covered != resolution)
            LX_CORE_WARN("A terrain of {0} heights a side is cut down to {1}, which its tiles of {2} quads fit into.", resolution, covered, m_TileQuads);

        const uint32_t texels = GetHeightTexels();
        m_Tiles.resize(static_cast<size_t>(m_TilesPerSide) * m_TilesPerSide);
        for (uint32_t tileZ = 0; tileZ < m_TilesPerSide; ++tileZ)
        {
            for (uint32_t tileX = 0; tileX < m_TilesPerSide; ++tileX)
            {
                Tile& tile = m_Tiles[static_cast<size_t>(tileZ) * m_TilesPerSide + tileX];
                tile.Heights.resize(static_cast<size_t>(texels) * texels);
                for (uint32_t z = 0; z < texels; ++z)
                {
                    const size_t row = static_cast<size_t>(InWhole(tileZ, m_TileQuads, z, covered)) * resolution;
                    for (uint32_t x = 0; x < texels; ++x)
                        tile.Heights[static_cast<size_t>(z) * texels + x] = heights[row + InWhole(tileX, m_TileQuads, x, covered)];
                }
                MeasurePatches(tile);
            }
        }

        CreateRenderResources();
    }

    Terrain::Terrain(const std::string& filePath)
        : Asset(filePath)
    {
    }

    Terrain::~Terrain() = default;

    std::shared_ptr<Terrain> Terrain::LoadFromFile(const std::filesystem::path& filePath)
    {
        auto terrain = std::make_shared<Terrain>(filePath.string());
        if (!terrain->LoadSourceData() || !terrain->CreateRenderResources())
            return nullptr;
        return terrain;
    }

    bool Terrain::SetTiling(uint32_t resolution, uint32_t tileQuads)
    {
        if (resolution <= TerrainQuadsPerPatch)
            return false;

        // A tile is a power of two of patches long, and no longer than the terrain.
        const uint32_t quads = resolution - 1;
        const uint32_t patchesPerTile = std::bit_floor(std::max(std::min(tileQuads, quads) / TerrainQuadsPerPatch, 1u));
        m_TileQuads = patchesPerTile * TerrainQuadsPerPatch;
        m_TilesPerSide = quads / m_TileQuads;
        return m_TilesPerSide > 0;
    }

    uint32_t Terrain::GetLevelCount() const
    {
        return m_TileQuads == 0 ? 0 : static_cast<uint32_t>(std::bit_width(m_TileQuads / TerrainQuadsPerPatch));
    }

    void Terrain::MeasurePatches(Tile& tile) const
    {
        // The smallest patches look at the heights themselves, their own edges included. Every larger
        // patch covers four of the level below.
        const uint32_t texels = GetHeightTexels();
        const uint32_t patches = GetPatchesPerSide(0);
        tile.PatchHeights.assign(GetLevelCount(), {});
        tile.PatchHeights[0].resize(static_cast<size_t>(patches) * patches);
        for (uint32_t patchZ = 0; patchZ < patches; ++patchZ)
        {
            for (uint32_t patchX = 0; patchX < patches; ++patchX)
            {
                StoredRange& range = tile.PatchHeights[0][static_cast<size_t>(patchZ) * patches + patchX];
                for (uint32_t z = patchZ * TerrainQuadsPerPatch; z <= (patchZ + 1) * TerrainQuadsPerPatch; ++z)
                {
                    for (uint32_t x = patchX * TerrainQuadsPerPatch; x <= (patchX + 1) * TerrainQuadsPerPatch; ++x)
                    {
                        const uint16_t height = tile.Heights[static_cast<size_t>(z + TerrainTileBorder) * texels + x + TerrainTileBorder];
                        range.Lowest = std::min(range.Lowest, height);
                        range.Highest = std::max(range.Highest, height);
                    }
                }
            }
        }
        for (uint32_t level = 1; level < tile.PatchHeights.size(); ++level)
        {
            const uint32_t perSide = patches >> level;
            const uint32_t below = perSide * 2;
            tile.PatchHeights[level].resize(static_cast<size_t>(perSide) * perSide);
            for (uint32_t z = 0; z < perSide; ++z)
            {
                for (uint32_t x = 0; x < perSide; ++x)
                {
                    StoredRange& range = tile.PatchHeights[level][static_cast<size_t>(z) * perSide + x];
                    for (uint32_t child = 0; child < 4; ++child)
                    {
                        const StoredRange& part = tile.PatchHeights[level - 1][static_cast<size_t>(z * 2 + child / 2) * below + x * 2 + child % 2];
                        range.Lowest = std::min(range.Lowest, part.Lowest);
                        range.Highest = std::max(range.Highest, part.Highest);
                    }
                }
            }
        }
    }

    Terrain::HeightRange Terrain::GetHeightsOfPatch(uint32_t tileX, uint32_t tileZ, uint32_t level, uint32_t x, uint32_t z) const
    {
        const StoredRange& range = TileAt(tileX, tileZ).PatchHeights[level][static_cast<size_t>(z) * GetPatchesPerSide(level) + x];
        return { ToHeight(range.Lowest), ToHeight(range.Highest) };
    }

    float Terrain::GetHeightAt(uint32_t x, uint32_t z) const
    {
        if (m_Tiles.empty())
            return m_Lowest;

        // The last place of the terrain is the last of its last tile, not the first of one beyond it.
        x = std::min(x, GetResolution() - 1);
        z = std::min(z, GetResolution() - 1);
        const uint32_t tileX = std::min(x / m_TileQuads, m_TilesPerSide - 1);
        const uint32_t tileZ = std::min(z / m_TileQuads, m_TilesPerSide - 1);
        const size_t inTile = static_cast<size_t>(z - tileZ * m_TileQuads + TerrainTileBorder) * GetHeightTexels() + (x - tileX * m_TileQuads + TerrainTileBorder);
        return ToHeight(TileAt(tileX, tileZ).Heights[inTile]);
    }

    void Terrain::SetLayers(std::span<const TerrainLayer> layers, uint32_t resolution, std::span<const uint8_t> weights)
    {
        m_LayerCount = 0;
        m_WeightTileResolution = 0;
        for (Tile& tile : m_Tiles)
        {
            tile.WeightSlot.reset();
            tile.WeightMap.reset();
            tile.Weights.clear();
        }

        if (m_Tiles.empty() || resolution == 0 || resolution % m_TilesPerSide != 0 || weights.size() < static_cast<size_t>(resolution) * resolution * BytesPerWeight)
        {
            LX_CORE_ERROR("A terrain's weights need four bytes for every place, and a resolution that its {0} tiles a side divide. It keeps its plain colours.", m_TilesPerSide);
            return;
        }

        // A layer without a material ends the list: the shader reads the weights of the first ones only.
        for (const TerrainLayer& layer : layers.first(std::min(layers.size(), TerrainLayerCount)))
        {
            if (!layer.Material)
                break;
            m_Layers[m_LayerCount++] = layer;
        }
        if (m_LayerCount == 0)
            return;

        m_WeightTileResolution = resolution / m_TilesPerSide;
        const uint32_t texels = GetWeightTexels();
        for (uint32_t tileZ = 0; tileZ < m_TilesPerSide; ++tileZ)
        {
            for (uint32_t tileX = 0; tileX < m_TilesPerSide; ++tileX)
            {
                Tile& tile = m_Tiles[static_cast<size_t>(tileZ) * m_TilesPerSide + tileX];
                tile.Weights.resize(static_cast<size_t>(texels) * texels * BytesPerWeight);
                for (uint32_t z = 0; z < texels; ++z)
                {
                    const size_t row = static_cast<size_t>(InWhole(tileZ, m_WeightTileResolution, z, resolution)) * resolution;
                    for (uint32_t x = 0; x < texels; ++x)
                    {
                        const uint8_t* from = &weights[(row + InWhole(tileX, m_WeightTileResolution, x, resolution)) * BytesPerWeight];
                        std::copy_n(from, BytesPerWeight, &tile.Weights[(static_cast<size_t>(z) * texels + x) * BytesPerWeight]);
                    }
                }
            }
        }

        if (!UploadWeights())
            m_LayerCount = 0;
    }

    void Terrain::SetScatterRules(std::span<const ScatterRule> rules)
    {
        m_ScatterRules.assign(rules.begin(), rules.end());
        ++m_ScatterVersion;
    }

    TerrainTileCopy Terrain::CopyTile(uint32_t tileX, uint32_t tileZ) const
    {
        const Tile& tile = TileAt(tileX, tileZ);
        TerrainTileCopy copy;
        copy.Size = GetTileSize();
        copy.Origin = glm::vec2(static_cast<float>(tileX), static_cast<float>(tileZ)) * copy.Size;
        copy.Spacing = GetSpacing();
        copy.Lowest = m_Lowest;
        copy.Highest = m_Highest;
        copy.Border = TerrainTileBorder;
        copy.HeightTexels = GetHeightTexels();
        copy.Heights = tile.Heights;
        if (m_LayerCount > 0)
        {
            copy.WeightTexels = GetWeightTexels();
            copy.Weights = tile.Weights;
        }
        return copy;
    }

    float Terrain::GetHeightAtPlace(float x, float z) const
    {
        if (m_Tiles.empty())
            return m_Lowest;

        const float last = static_cast<float>(GetResolution() - 1);
        const float inStepsX = std::clamp(x / GetSpacing(), 0.0f, last);
        const float inStepsZ = std::clamp(z / GetSpacing(), 0.0f, last);
        const uint32_t quadX = std::min(static_cast<uint32_t>(inStepsX), GetResolution() - 2);
        const uint32_t quadZ = std::min(static_cast<uint32_t>(inStepsZ), GetResolution() - 2);
        const float alongX = inStepsX - static_cast<float>(quadX);
        const float alongZ = inStepsZ - static_cast<float>(quadZ);

        // A quad is two triangles, split from its first corner to the one opposite. Has to match the
        // patch grid of TerrainRenderer, which matches the height field of the physics.
        const float first = GetHeightAt(quadX, quadZ);
        const float opposite = GetHeightAt(quadX + 1, quadZ + 1);
        if (alongZ >= alongX)
        {
            const float alongZOnly = GetHeightAt(quadX, quadZ + 1);
            return first + alongZ * (alongZOnly - first) + alongX * (opposite - alongZOnly);
        }
        const float alongXOnly = GetHeightAt(quadX + 1, quadZ);
        return first + alongX * (alongXOnly - first) + alongZ * (opposite - alongXOnly);
    }

    void Terrain::SetLayers(std::span<const TerrainLayer> layers)
    {
        m_LayerCount = 0;
        for (const TerrainLayer& layer : layers.first(std::min(layers.size(), TerrainLayerCount)))
        {
            if (!layer.Material)
                break;
            m_Layers[m_LayerCount++] = layer;
        }
        IncrementVersion();
        if (m_LayerCount == 0 || m_Tiles.empty())
            return;

        // No weights yet: a few places per tile are enough to say "the first layer, everywhere".
        const auto hasWeights = [this](const Tile& tile) { return tile.Weights.size() == static_cast<size_t>(GetWeightTexels()) * GetWeightTexels() * BytesPerWeight; };
        if (m_WeightTileResolution == 0 || !std::ranges::all_of(m_Tiles, hasWeights))
        {
            m_WeightTileResolution = 8;
            const size_t texels = GetWeightTexels();
            for (Tile& tile : m_Tiles)
            {
                tile.Weights.assign(texels * texels * BytesPerWeight, 0);
                for (size_t place = 0; place < texels * texels; ++place)
                    tile.Weights[place * BytesPerWeight] = 255;
                tile.WeightSlot.reset();
                tile.WeightMap.reset();
            }
        }

        const bool isUploaded = std::ranges::all_of(m_Tiles, [](const Tile& tile) { return tile.WeightMap != nullptr; });
        if (!isUploaded && !UploadWeights())
            m_LayerCount = 0;
    }

    bool Terrain::Reload()
    {
        if (m_FilePath.empty())
            return true;

        m_LayerCount = 0;
        m_IsOnGpu = false;
        return LoadSourceData() && CreateRenderResources();
    }

    bool Terrain::UploadWeights()
    {
        // Smaller sizes for ground far away, like any texture.
        TextureSpecification specification;
        specification.Width = GetWeightTexels();
        specification.Height = GetWeightTexels();
        specification.Format = TextureFormat::RGBA8;
        specification.GenerateMips = true;
        specification.DebugName = "TerrainWeights";

        Renderer& renderer = Engine::Get().GetRenderer();
        for (Tile& tile : m_Tiles)
        {
            // The place in the table goes before the picture it points at.
            tile.WeightSlot.reset();
            tile.WeightMap = renderer.CreateTexture(specification, tile.Weights.data());
            if (!tile.WeightMap)
                return false;
            tile.WeightSlot = std::make_unique<TextureSlot>(renderer.AddToTextureTable(*tile.WeightMap));
        }
        IncrementVersion();
        return true;
    }

    uint32_t Terrain::GetHeightTableIndex(uint32_t tileX, uint32_t tileZ) const
    {
        const Tile& tile = TileAt(tileX, tileZ);
        return tile.HeightSlot ? tile.HeightSlot->Index() : 0;
    }

    uint32_t Terrain::GetWeightTableIndex(uint32_t tileX, uint32_t tileZ) const
    {
        const Tile& tile = TileAt(tileX, tileZ);
        return tile.WeightSlot ? tile.WeightSlot->Index() : 0;
    }

    bool Terrain::Save(const std::filesystem::path& filePath) const
    {
        TerrainSource source;
        source.Size = m_Size;
        source.Lowest = m_Lowest;
        source.Highest = m_Highest;
        source.TilesPerSide = m_TilesPerSide;
        source.TileQuads = m_TileQuads;
        source.WeightTileResolution = m_LayerCount > 0 ? m_WeightTileResolution : 0;
        for (const TerrainLayer& layer : GetLayers())
            source.Layers.push_back({ layer.Material->GetHandle(), layer.TileSize });

        // A rule is saved by the handle of its mesh and, like a pending one, keeps none itself.
        for (const ScatterRule& rule : m_ScatterRules)
        {
            if (!rule.Mesh)
                continue;

            TerrainSource::Scatter& saved = source.ScatterRules.emplace_back();
            saved.Mesh = rule.Mesh->GetHandle();
            saved.Rule = rule;
            saved.Rule.Mesh = nullptr;
        }
        for (const Tile& tile : m_Tiles)
            source.Tiles.push_back({ tile.Heights, m_LayerCount > 0 ? tile.Weights : std::vector<uint8_t>() });
        return TerrainSerializer::Serialize(filePath, source);
    }

    bool Terrain::LoadSourceData()
    {
        // A terrain made in code has nothing to read.
        if (m_FilePath.empty())
            return true;

        TerrainSource source;
        if (!TerrainSerializer::Deserialize(m_FilePath, source))
            return false;

        m_Size = source.Size;
        m_Lowest = source.Lowest;
        m_Highest = source.Highest;
        if (!SetTiling(source.TilesPerSide * source.TileQuads + 1, source.TileQuads) || m_TilesPerSide != source.TilesPerSide || m_TileQuads != source.TileQuads)
        {
            LX_CORE_ERROR("Terrain {0} is cut into tiles in a way the engine cannot draw: {1} tiles a side of {2} quads.", m_FilePath, source.TilesPerSide, source.TileQuads);
            return false;
        }

        m_WeightTileResolution = source.WeightTileResolution;
        m_Tiles.clear();
        m_Tiles.resize(source.Tiles.size());
        for (size_t index = 0; index < source.Tiles.size(); ++index)
        {
            m_Tiles[index].Heights = std::move(source.Tiles[index].Heights);
            m_Tiles[index].Weights = std::move(source.Tiles[index].Weights);
            MeasurePatches(m_Tiles[index]);
        }

        m_PendingLayers.clear();
        for (const TerrainSource::Layer& layer : source.Layers)
            m_PendingLayers.push_back({ layer.Material, layer.TileSize });

        m_PendingRules.clear();
        for (const TerrainSource::Scatter& rule : source.ScatterRules)
            m_PendingRules.push_back({ rule.Mesh, rule.Rule });
        m_HasPendingRules = true;
        return true;
    }

    bool Terrain::CreateRenderResources()
    {
        if (m_Tiles.empty())
            return false;

        Renderer& renderer = Engine::Get().GetRenderer();

        // Smaller sizes of the heights are for the shading of what is far away, where several heights
        // fall into a pixel. The shape itself is always read from the largest.
        TextureSpecification specification;
        specification.Width = GetHeightTexels();
        specification.Height = GetHeightTexels();
        specification.Format = TextureFormat::R16;
        specification.GenerateMips = true;
        specification.DebugName = "TerrainHeights";

        m_IsOnGpu = false;
        for (Tile& tile : m_Tiles)
        {
            // The place in the table goes before the picture it points at.
            tile.HeightSlot.reset();
            tile.HeightMap = renderer.CreateTexture(specification, reinterpret_cast<const unsigned char*>(tile.Heights.data()));
            if (!tile.HeightMap)
                return false;
            tile.HeightSlot = std::make_unique<TextureSlot>(renderer.AddToTextureTable(*tile.HeightMap));
        }

        // Read between the heights, and never beyond a tile's picture.
        m_HeightSampler = renderer.SamplerIndex(SamplerSettings{ TextureWrap::Clamp, TextureFilter::Trilinear, false });
        m_IsOnGpu = true;
        IncrementVersion();

        // The rules of a terrain that was read from a file: their meshes are looked up now. A mesh may
        // still be loading: whoever draws the scattered things waits for it.
        if (m_HasPendingRules)
        {
            AssetManager& assets = Engine::Get().GetAssetManager();
            std::vector<ScatterRule> rules;
            for (PendingRule& pending : m_PendingRules)
            {
                pending.Rule.Mesh = assets.GetAsset<StaticMesh>(pending.Mesh);
                if (pending.Rule.Mesh)
                    rules.push_back(std::move(pending.Rule));
                else
                    LX_CORE_ERROR("Terrain {0}: the mesh of a scatter rule was not found. The rule is left out.", m_FilePath);
            }
            m_PendingRules.clear();
            m_HasPendingRules = false;
            SetScatterRules(rules);
        }

        // The layers of a terrain that was read from a file: their materials are looked up now, on the
        // main thread. One that cannot be found ends the list, as in SetLayers.
        if (!m_PendingLayers.empty())
        {
            AssetManager& assets = Engine::Get().GetAssetManager();
            m_LayerCount = 0;
            for (const PendingLayer& pending : m_PendingLayers)
            {
                if (m_LayerCount == TerrainLayerCount)
                    break;

                std::shared_ptr<Material> material = assets.GetAsset<Material>(pending.Material, AssetLoadMode::Blocking);
                if (!material)
                {
                    LX_CORE_ERROR("Terrain {0}: the material of layer {1} was not found. The layers after it are left out.", m_FilePath, m_LayerCount);
                    break;
                }
                m_Layers[m_LayerCount++] = { std::move(material), pending.TileSize };
            }
            m_PendingLayers.clear();

            const size_t weightBytes = static_cast<size_t>(GetWeightTexels()) * GetWeightTexels() * BytesPerWeight;
            const bool hasWeights = m_WeightTileResolution > 0 && std::ranges::all_of(m_Tiles, [weightBytes](const Tile& tile) { return tile.Weights.size() >= weightBytes; });
            if (m_LayerCount > 0 && (!hasWeights || !UploadWeights()))
                m_LayerCount = 0;
        }
        return true;
    }
}
