#include "Terrain.h"

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
    Terrain::Terrain(uint32_t resolution, float size, float lowest, float highest, std::vector<uint16_t> heights)
        : m_Size(size), m_Lowest(lowest), m_Highest(highest)
    {
        if (!SetHeights(resolution, std::move(heights)))
        {
            SetState(AssetState::Error);
            return;
        }

        CreateRenderResources();
    }

    Terrain::Terrain(const std::string& filePath)
        : Asset(filePath)
    {
    }

    std::shared_ptr<Terrain> Terrain::LoadFromFile(const std::filesystem::path& filePath)
    {
        auto terrain = std::make_shared<Terrain>(filePath.string());
        if (!terrain->LoadSourceData() || !terrain->CreateRenderResources())
            return nullptr;
        return terrain;
    }

    bool Terrain::Save(const std::filesystem::path& filePath) const
    {
        TerrainSource source;
        source.Resolution = m_Resolution;
        source.Size = m_Size;
        source.Lowest = m_Lowest;
        source.Highest = m_Highest;
        source.Heights = m_Heights;
        source.WeightResolution = m_WeightResolution;
        source.Weights = m_Weights;
        for (const TerrainLayer& layer : GetLayers())
            source.Layers.push_back({ layer.Material->GetHandle(), layer.TileSize });
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
        if (!SetHeights(source.Resolution, std::move(source.Heights)))
            return false;

        m_WeightResolution = source.WeightResolution;
        m_Weights = std::move(source.Weights);
        m_PendingLayers.clear();
        for (const TerrainSource::Layer& layer : source.Layers)
            m_PendingLayers.push_back({ layer.Material, layer.TileSize });
        return true;
    }

    bool Terrain::SetHeights(uint32_t resolution, std::vector<uint16_t> heights)
    {
        // The largest resolution that fits both the heights given and the patches, see the class.
        const uint32_t patches = resolution > TerrainQuadsPerPatch ? std::bit_floor((resolution - 1) / TerrainQuadsPerPatch) : 0;
        if (patches == 0 || heights.size() < static_cast<size_t>(resolution) * resolution)
        {
            LX_CORE_ERROR("A terrain needs at least {0} heights a side, and one for every place. It stays empty.", TerrainQuadsPerPatch + 1);
            return false;
        }

        m_Resolution = patches * TerrainQuadsPerPatch + 1;
        if (m_Resolution == resolution)
        {
            m_Heights = std::move(heights);
        }
        else
        {
            LX_CORE_WARN("A terrain of {0} heights a side is cut down to {1}, the next size its patches fit into.", resolution, m_Resolution);
            m_Heights.resize(static_cast<size_t>(m_Resolution) * m_Resolution);
            for (uint32_t z = 0; z < m_Resolution; ++z)
                std::copy_n(heights.begin() + static_cast<size_t>(z) * resolution, m_Resolution, m_Heights.begin() + static_cast<size_t>(z) * m_Resolution);
        }

        // The smallest patches look at the heights themselves, their own edges included. Every larger
        // patch covers four of the level below.
        const uint32_t levelCount = static_cast<uint32_t>(std::bit_width(patches));
        m_PatchHeights.resize(levelCount);
        m_PatchHeights[0].resize(static_cast<size_t>(patches) * patches);
        for (uint32_t patchZ = 0; patchZ < patches; ++patchZ)
        {
            for (uint32_t patchX = 0; patchX < patches; ++patchX)
            {
                StoredRange& range = m_PatchHeights[0][static_cast<size_t>(patchZ) * patches + patchX];
                for (uint32_t z = patchZ * TerrainQuadsPerPatch; z <= (patchZ + 1) * TerrainQuadsPerPatch; ++z)
                {
                    for (uint32_t x = patchX * TerrainQuadsPerPatch; x <= (patchX + 1) * TerrainQuadsPerPatch; ++x)
                    {
                        const uint16_t height = m_Heights[static_cast<size_t>(z) * m_Resolution + x];
                        range.Lowest = std::min(range.Lowest, height);
                        range.Highest = std::max(range.Highest, height);
                    }
                }
            }
        }
        for (uint32_t level = 1; level < levelCount; ++level)
        {
            const uint32_t perSide = patches >> level;
            const uint32_t below = perSide * 2;
            m_PatchHeights[level].resize(static_cast<size_t>(perSide) * perSide);
            for (uint32_t z = 0; z < perSide; ++z)
            {
                for (uint32_t x = 0; x < perSide; ++x)
                {
                    StoredRange& range = m_PatchHeights[level][static_cast<size_t>(z) * perSide + x];
                    for (uint32_t child = 0; child < 4; ++child)
                    {
                        const StoredRange& part = m_PatchHeights[level - 1][static_cast<size_t>(z * 2 + child / 2) * below + x * 2 + child % 2];
                        range.Lowest = std::min(range.Lowest, part.Lowest);
                        range.Highest = std::max(range.Highest, part.Highest);
                    }
                }
            }
        }

        return true;
    }

    Terrain::~Terrain() = default;

    Terrain::HeightRange Terrain::GetHeightsOfPatch(uint32_t level, uint32_t x, uint32_t z) const
    {
        const StoredRange& range = m_PatchHeights[level][static_cast<size_t>(z) * GetPatchesPerSide(level) + x];
        return { ToHeight(range.Lowest), ToHeight(range.Highest) };
    }

    float Terrain::GetHeightAt(uint32_t x, uint32_t z) const
    {
        if (m_Heights.empty())
            return m_Lowest;
        return ToHeight(m_Heights[static_cast<size_t>(std::min(z, m_Resolution - 1)) * m_Resolution + std::min(x, m_Resolution - 1)]);
    }

    void Terrain::SetLayers(std::span<const TerrainLayer> layers, uint32_t resolution, std::vector<uint8_t> weights)
    {
        m_LayerCount = 0;
        m_WeightSlot.reset();
        m_WeightMap.reset();

        if (weights.size() < static_cast<size_t>(resolution) * resolution * TerrainLayerCount)
        {
            LX_CORE_ERROR("A terrain's weights need four bytes for every place. It keeps its plain colours.");
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

        m_WeightResolution = resolution;
        m_Weights = std::move(weights);
        if (!UploadWeights())
            m_LayerCount = 0;
    }

    bool Terrain::UploadWeights()
    {
        // Smaller sizes for ground far away, like any texture.
        TextureSpecification specification;
        specification.Width = m_WeightResolution;
        specification.Height = m_WeightResolution;
        specification.Format = TextureFormat::RGBA8;
        specification.GenerateMips = true;
        specification.DebugName = "TerrainWeights";

        Renderer& renderer = Engine::Get().GetRenderer();
        m_WeightSlot.reset();
        m_WeightMap = renderer.CreateTexture(specification, m_Weights.data());
        if (!m_WeightMap)
            return false;

        m_WeightSlot = std::make_unique<TextureSlot>(renderer.AddToTextureTable(*m_WeightMap));
        IncrementVersion();
        return true;
    }

    uint32_t Terrain::GetWeightTableIndex() const
    {
        return m_WeightSlot ? m_WeightSlot->Index() : 0;
    }

    uint32_t Terrain::GetHeightTableIndex() const
    {
        return m_HeightSlot ? m_HeightSlot->Index() : 0;
    }

    bool Terrain::CreateRenderResources()
    {
        if (m_Heights.empty())
            return false;

        Renderer& renderer = Engine::Get().GetRenderer();

        // Smaller sizes of the heights are for the shading of what is far away, where several heights
        // fall into a pixel. The shape itself is always read from the largest.
        TextureSpecification specification;
        specification.Width = m_Resolution;
        specification.Height = m_Resolution;
        specification.Format = TextureFormat::R16;
        specification.GenerateMips = true;
        specification.DebugName = "TerrainHeights";

        // The place in the table goes before the picture it points at.
        m_HeightSlot.reset();
        m_HeightMap = renderer.CreateTexture(specification, reinterpret_cast<const unsigned char*>(m_Heights.data()));
        if (!m_HeightMap)
            return false;

        // Read between the heights, and never beyond the terrain's edge.
        m_HeightSlot = std::make_unique<TextureSlot>(renderer.AddToTextureTable(*m_HeightMap));
        m_HeightSampler = renderer.SamplerIndex(SamplerSettings{ TextureWrap::Clamp, TextureFilter::Trilinear, false });
        IncrementVersion();

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

            if (m_LayerCount > 0 && (m_Weights.size() < static_cast<size_t>(m_WeightResolution) * m_WeightResolution * TerrainLayerCount || !UploadWeights()))
                m_LayerCount = 0;
        }
        return true;
    }
}
