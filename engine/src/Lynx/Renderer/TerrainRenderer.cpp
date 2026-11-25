#include "TerrainRenderer.h"

#include "GraphicsContext.h"
#include "Vulkan/UploadQueue.h"
#include "Lynx/Asset/Terrain.h"

#include <algorithm>
#include <array>

namespace Lynx
{
    namespace
    {
        // The smallest patches are drawn out to this many of their own lengths from the camera, and
        // each larger level twice as far. More draws finer ground farther out, at four times the
        // triangles for twice the number.
        constexpr float FinestRangeInPatches = 2.5f;

        constexpr uint32_t QuarterCount = 4;

        // What the shader knows of a patch. Has to match TerrainPatch in Terrain.slang.
        struct GpuPatch
        {
            // The corner with the smallest x and z, counted from the terrain's own, and the length of
            // the patch the grid is stretched over. For a quarter that is still the whole patch.
            glm::vec2 Origin;
            float Size;

            // The level in the low 8 bits, the terrain's place in the list of terrains above them.
            uint32_t LevelAndTerrain;
        };

        // What the shader knows of a terrain. Has to match TerrainData in Terrain.slang.
        struct GpuTerrain
        {
            glm::vec3 Origin;
            float Size;
            float Lowest;
            float HeightRange;
            uint32_t HeightTexture;
            uint32_t HeightSampler;
            float Resolution;

            // How far from the camera the smallest patches are drawn.
            float FinestRange;
            float Padding[2];
        };

        template<typename Entry>
        void UploadList(GraphicsContext& context, UploadQueue& uploads, GpuBuffer& buffer, std::span<const Entry> entries, const char* name)
        {
            if (entries.empty())
                return;

            if (buffer.Size() < entries.size_bytes())
            {
                // Half as much again as needed, so a list that grows a little each frame does not reallocate each frame.
                GpuBufferDesc desc;
                desc.Size = entries.size_bytes() + entries.size_bytes() / 2;
                desc.Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
                desc.Name = name;
                buffer = GpuBuffer(context, desc);
            }
            uploads.Upload(buffer.Handle(), 0, std::as_bytes(entries));
        }

        // True if any of a box lies within a distance of a point.
        bool ReachesWithin(const AABB& box, const glm::vec3& point, float distance)
        {
            const glm::vec3 nearest = glm::clamp(point, box.Min, box.Max);
            const glm::vec3 away = nearest - point;
            return glm::dot(away, away) <= distance * distance;
        }
    }

    // What picking the patches of one terrain needs and gives.
    struct TerrainRenderer::Selection
    {
        const Terrain* Asset = nullptr;
        glm::vec3 Origin{ 0.0f };
        uint32_t TerrainIndex = 0;
        const Frustum* View = nullptr;
        glm::vec3 Camera{ 0.0f };
        float FinestRange = 0.0f;

        // Whole patches first, then the four quarters.
        std::array<std::vector<GpuPatch>, QuarterCount + 1>* Kinds = nullptr;

        float PatchSize(uint32_t level) const
        {
            return Asset->GetSize() / static_cast<float>(Asset->GetPatchesPerSide(level));
        }

        AABB BoxOf(uint32_t level, uint32_t x, uint32_t z) const
        {
            const float size = PatchSize(level);
            const Terrain::HeightRange heights = Asset->GetHeightsOfPatch(level, x, z);
            const glm::vec3 corner = Origin + glm::vec3(static_cast<float>(x) * size, 0.0f, static_cast<float>(z) * size);
            return AABB(corner + glm::vec3(0.0f, heights.Lowest, 0.0f), corner + glm::vec3(size, heights.Highest, size));
        }
    };

    TerrainRenderer::TerrainRenderer(GraphicsContext& context, UploadQueue& uploads)
        : m_Context(context), m_Uploads(uploads)
    {
        // The grid has one more vertex a side than quads, numbered row by row. Two triangles per quad,
        // counter-clockwise seen from above.
        constexpr uint32_t quads = TerrainQuadsPerPatch;
        constexpr uint32_t half = quads / 2;
        std::vector<uint16_t> indices;
        indices.reserve(static_cast<size_t>(quads) * quads * 6);
        for (uint32_t quarter = 0; quarter < QuarterCount; ++quarter)
        {
            const uint32_t firstX = (quarter % 2) * half;
            const uint32_t firstZ = (quarter / 2) * half;
            for (uint32_t z = firstZ; z < firstZ + half; ++z)
            {
                for (uint32_t x = firstX; x < firstX + half; ++x)
                {
                    const auto vertex = [](uint32_t vx, uint32_t vz) { return static_cast<uint16_t>(vz * (quads + 1) + vx); };
                    indices.insert(indices.end(), { vertex(x, z), vertex(x, z + 1), vertex(x + 1, z), vertex(x + 1, z), vertex(x, z + 1), vertex(x + 1, z + 1) });
                }
            }
        }
        m_IndicesPerQuarter = static_cast<uint32_t>(indices.size()) / QuarterCount;

        GpuBufferDesc desc;
        desc.Size = indices.size() * sizeof(uint16_t);
        desc.Usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        desc.Name = "TerrainPatchIndices";
        m_Indices = GpuBuffer(context, desc);
        uploads.Upload(m_Indices.Handle(), 0, std::as_bytes(std::span<const uint16_t>(indices)));
    }

    void TerrainRenderer::Select(Selection& selection, uint32_t level, uint32_t x, uint32_t z) const
    {
        const AABB box = selection.BoxOf(level, x, z);
        if (!selection.View->IsOnFrustum(box))
            return;

        const auto add = [&](size_t kind)
        {
            const float size = selection.PatchSize(level);
            (*selection.Kinds)[kind].push_back({ glm::vec2(static_cast<float>(x), static_cast<float>(z)) * size, size, level | (selection.TerrainIndex << 8) });
        };

        // The distance out to which the level below is drawn. A patch that lies beyond it everywhere
        // is drawn as it is, and so is one of the smallest.
        const float finerRange = level == 0 ? 0.0f : selection.FinestRange * static_cast<float>(1u << (level - 1));
        if (level == 0 || !ReachesWithin(box, selection.Camera, finerRange))
        {
            add(0);
            return;
        }

        // Otherwise each quarter on its own: finer where it reaches into that distance, and as a
        // quarter of this patch where it does not.
        for (uint32_t quarter = 0; quarter < QuarterCount; ++quarter)
        {
            const uint32_t childX = x * 2 + quarter % 2;
            const uint32_t childZ = z * 2 + quarter / 2;
            const AABB childBox = selection.BoxOf(level - 1, childX, childZ);
            if (ReachesWithin(childBox, selection.Camera, finerRange))
                Select(selection, level - 1, childX, childZ);
            else if (selection.View->IsOnFrustum(childBox))
                add(1 + quarter);
        }
    }

    TerrainViews TerrainRenderer::Prepare(std::span<const ExtractedTerrain> terrains, const Frustum& camera, std::span<const Frustum, ShadowCascadeCount> sun,
        const glm::vec3& cameraPosition)
    {
        std::vector<const ExtractedTerrain*> drawn;
        std::vector<GpuTerrain> gpuTerrains;
        for (const ExtractedTerrain& terrain : terrains)
        {
            if (!terrain.Asset || !terrain.Asset->IsOnGpu())
                continue;

            const Terrain& asset = *terrain.Asset;
            GpuTerrain& gpu = gpuTerrains.emplace_back();
            gpu.Origin = terrain.Origin;
            gpu.Size = asset.GetSize();
            gpu.Lowest = asset.GetLowest();
            gpu.HeightRange = asset.GetHighest() - asset.GetLowest();
            gpu.HeightTexture = asset.GetHeightTableIndex();
            gpu.HeightSampler = asset.GetHeightSamplerIndex();
            gpu.Resolution = static_cast<float>(asset.GetResolution());
            gpu.FinestRange = asset.GetSize() / static_cast<float>(asset.GetPatchesPerSide(0)) * FinestRangeInPatches;
            drawn.push_back(&terrain);
        }

        TerrainViews views;
        if (drawn.empty())
            return views;

        // One list of the patches of all views. Within a view kind follows kind, so that a kind is one
        // run of the list and one draw call.
        std::vector<GpuPatch> patches;
        const auto pick = [&](const Frustum& view)
        {
            std::array<std::vector<GpuPatch>, QuarterCount + 1> kinds;
            for (uint32_t index = 0; index < drawn.size(); ++index)
            {
                const Terrain& asset = *drawn[index]->Asset;
                const uint32_t topLevel = asset.GetLevelCount() - 1;

                Selection selection;
                selection.Asset = &asset;
                selection.Origin = drawn[index]->Origin;
                selection.TerrainIndex = index;
                selection.View = &view;
                selection.Camera = cameraPosition;
                selection.FinestRange = gpuTerrains[index].FinestRange;
                selection.Kinds = &kinds;

                // The largest patches are drawn however far away they are.
                const uint32_t perSide = asset.GetPatchesPerSide(topLevel);
                for (uint32_t z = 0; z < perSide; ++z)
                {
                    for (uint32_t x = 0; x < perSide; ++x)
                        Select(selection, topLevel, x, z);
                }
            }

            TerrainDraws draws;
            for (size_t kind = 0; kind < kinds.size(); ++kind)
            {
                TerrainDraws::Run& run = draws.Runs[kind];
                run.FirstIndex = kind == 0 ? 0 : static_cast<uint32_t>(kind - 1) * m_IndicesPerQuarter;
                run.IndexCount = kind == 0 ? m_IndicesPerQuarter * QuarterCount : m_IndicesPerQuarter;
                run.FirstPatch = static_cast<uint32_t>(patches.size());
                run.PatchCount = static_cast<uint32_t>(kinds[kind].size());
                patches.insert(patches.end(), kinds[kind].begin(), kinds[kind].end());

                draws.PatchCount += run.PatchCount;
                draws.TriangleCount += static_cast<uint64_t>(run.PatchCount) * (run.IndexCount / 3);
            }
            return draws;
        };

        views.Camera = pick(camera);
        for (size_t cascade = 0; cascade < sun.size(); ++cascade)
            views.Sun[cascade] = pick(sun[cascade]);
        if (patches.empty())
            return views;

        UploadList<GpuPatch>(m_Context, m_Uploads, m_Patches, patches, "TerrainPatches");
        UploadList<GpuTerrain>(m_Context, m_Uploads, m_Terrains, gpuTerrains, "Terrains");

        const auto finish = [this](TerrainDraws& draws)
        {
            draws.Indices = &m_Indices;
            draws.PatchAddress = m_Patches.Address();
            draws.TerrainAddress = m_Terrains.Address();
        };
        finish(views.Camera);
        for (TerrainDraws& draws : views.Sun)
            finish(draws);
        return views;
    }
}
