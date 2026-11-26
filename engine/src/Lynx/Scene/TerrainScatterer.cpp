#include "TerrainScatterer.h"

#include "Scene.h"
#include "Components/Components.h"
#include "Components/TerrainComponent.h"
#include "Lynx/Asset/StaticMesh.h"
#include "Lynx/Asset/Terrain.h"
#include "Lynx/Core/WorkerPool.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Lynx
{
    namespace
    {
        // A number that looks random for every input and is the same every time.
        uint32_t Hash(uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7FEB352Du;
            value ^= value >> 15;
            value *= 0x846CA68Bu;
            value ^= value >> 16;
            return value;
        }

        // From 0 to just under 1, for a square of the grid of points and one of the things drawn for it.
        float Chance(int32_t squareX, int32_t squareZ, uint32_t pattern, uint32_t what)
        {
            const uint32_t mixed = Hash(static_cast<uint32_t>(squareX) * 0x9E3779B1u ^ Hash(static_cast<uint32_t>(squareZ) + pattern * 0x85EBCA6Bu + what * 0xC2B2AE35u));
            return static_cast<float>(mixed >> 8) / 16777216.0f;
        }

        // How dense a rule's kind stands at a place, 0 to 1.
        float DensityAt(const ScatterRule& rule, float height, float slopeInDegrees, const std::array<float, 4>& layers)
        {
            if (height < rule.LowestHeight || height > rule.HighestHeight)
                return 0.0f;

            // Thinning out over the last fifth of the slopes it grows on.
            const float steepest = std::max(rule.SteepestSlope, 0.01f);
            const float onSlope = 1.0f - glm::smoothstep(0.8f * steepest, steepest, slopeInDegrees);

            float onGround = 0.0f;
            for (size_t layer = 0; layer < layers.size(); ++layer)
                onGround += layers[layer] * rule.LayerShares[layer];

            return std::clamp(rule.Density, 0.0f, 1.0f) * onSlope * std::clamp(onGround, 0.0f, 1.0f);
        }
    }

    std::vector<ScatterInstance> ScatterOverTile(const TerrainTileCopy& tile, const std::vector<ScatterRule>& rules, const glm::vec3& origin)
    {
        std::vector<ScatterInstance> instances;

        // Rules of one spacing are worked through together, the first of them leading.
        std::vector<bool> isDone(rules.size(), false);
        for (size_t leading = 0; leading < rules.size(); ++leading)
        {
            const float spacing = rules[leading].Spacing;
            if (isDone[leading] || spacing <= 0.0f)
                continue;

            std::vector<uint32_t> sharing;
            for (size_t index = leading; index < rules.size(); ++index)
            {
                if (rules[index].Spacing == spacing)
                {
                    isDone[index] = true;
                    if (rules[index].Mesh)
                        sharing.push_back(static_cast<uint32_t>(index));
                }
            }
            if (sharing.empty())
                continue;

            // The pattern belongs to the spacing, whichever rules use it.
            uint32_t pattern = 0;
            std::memcpy(&pattern, &spacing, sizeof(pattern));

            // The squares of the grid that reach into the tile. The grid is anchored at the terrain's
            // first corner, so the points are the same whichever tile looks at a square.
            const int32_t firstX = static_cast<int32_t>(std::floor(tile.Origin.x / spacing));
            const int32_t firstZ = static_cast<int32_t>(std::floor(tile.Origin.y / spacing));
            const int32_t lastX = static_cast<int32_t>(std::floor((tile.Origin.x + tile.Size) / spacing));
            const int32_t lastZ = static_cast<int32_t>(std::floor((tile.Origin.y + tile.Size) / spacing));
            for (int32_t squareZ = firstZ; squareZ <= lastZ; ++squareZ)
            {
                for (int32_t squareX = firstX; squareX <= lastX; ++squareX)
                {
                    // One point per square, anywhere in it. A point on the border between two tiles
                    // belongs to the one it lies in, so none is placed twice.
                    const float x = (static_cast<float>(squareX) + Chance(squareX, squareZ, pattern, 0)) * spacing - tile.Origin.x;
                    const float z = (static_cast<float>(squareZ) + Chance(squareX, squareZ, pattern, 1)) * spacing - tile.Origin.y;
                    if (x < 0.0f || z < 0.0f || x >= tile.Size || z >= tile.Size)
                        continue;

                    const float height = tile.HeightAt(x, z);
                    const float slope = glm::degrees(std::acos(std::clamp(tile.NormalAt(x, z).y, 0.0f, 1.0f)));
                    const std::array<float, 4> layers = tile.LayersAt(x, z);

                    // The rules lie on top of each other from 0 upwards, each as wide as its density
                    // here. The point falls to the one its threshold lies in, or to none.
                    const float threshold = Chance(squareX, squareZ, pattern, 2);
                    float below = 0.0f;
                    for (const uint32_t index : sharing)
                    {
                        const ScatterRule& rule = rules[index];
                        const float density = DensityAt(rule, height, slope, layers);
                        if (threshold >= below + density)
                        {
                            below += density;
                            continue;
                        }

                        const float size = glm::mix(rule.SmallestSize, rule.LargestSize, Chance(squareX, squareZ, pattern, 3));
                        const glm::vec3 position = origin + glm::vec3(tile.Origin.x + x, height, tile.Origin.y + z);
                        instances.push_back(ScatterInstance::At(position, Chance(squareX, squareZ, pattern, 4), size, index));
                        break;
                    }
                }
            }
        }

        return instances;
    }

    TerrainScatterer::TerrainScatterer() = default;

    // The pool goes first and waits for the tile that is in work, which writes to the list of finished ones.
    TerrainScatterer::~TerrainScatterer() = default;

    void TerrainScatterer::Update(Scene& scene)
    {
        entt::registry& registry = scene.Reg();

        // An entity that is gone, or has no terrain any more, loses what was scattered for it.
        for (auto it = m_Scattered.begin(); it != m_Scattered.end();)
        {
            if (registry.valid(it->first) && registry.all_of<TransformComponent, TerrainComponent>(it->first))
            {
                ++it;
                continue;
            }
            if (registry.valid(it->first))
                registry.remove<ScatterComponent>(it->first);
            m_TilesInWork -= it->second.TilesMissing;
            it = m_Scattered.erase(it);
        }

        // Nothing is listened for: the few terrains of a scene are looked at every frame, which also
        // catches a terrain that was reloaded, moved, or given other rules.
        const auto terrains = registry.view<TransformComponent, TerrainComponent>();
        for (const entt::entity entity : terrains)
        {
            const std::shared_ptr<Terrain> terrain = terrains.get<TerrainComponent>(entity).Terrain.Get();
            if (!terrain || !terrain->IsLoaded() || terrain->GetTilesPerSide() == 0)
                continue;

            // Like the renderer, scattering takes the entity's position as the terrain's first corner
            // and nothing else of its transform.
            const glm::vec3 origin(terrains.get<TransformComponent>(entity).GetWorldMatrix()[3]);
            const bool isKnown = m_Scattered.contains(entity);
            if (!isKnown && terrain->GetScatterRules().empty())
                continue;

            Scattered& scattered = m_Scattered[entity];
            const bool isCurrent = isKnown && scattered.Terrain == terrain->GetHandle() && scattered.Version == terrain->GetVersion()
                && scattered.RulesVersion == terrain->GetScatterVersion() && scattered.Origin == origin;
            if (isCurrent)
                continue;

            // Tiles still in work for what it was before are dropped when they come back.
            m_TilesInWork -= scattered.TilesMissing;
            scattered = Scattered();
            scattered.Terrain = terrain->GetHandle();
            scattered.Version = terrain->GetVersion();
            scattered.RulesVersion = terrain->GetScatterVersion();
            scattered.Origin = origin;
            scattered.Start = ++m_Starts;
            scattered.StartedAt = std::chrono::steady_clock::now();

            const std::vector<ScatterRule>& rules = terrain->GetScatterRules();
            if (rules.empty())
            {
                registry.remove<ScatterComponent>(entity);
                continue;
            }

            for (const ScatterRule& rule : rules)
                scattered.Kinds.push_back(rule.Mesh);

            const uint32_t tilesPerSide = terrain->GetTilesPerSide();
            scattered.Tiles.resize(static_cast<size_t>(tilesPerSide) * tilesPerSide);
            scattered.TilesMissing = static_cast<uint32_t>(scattered.Tiles.size());
            m_TilesInWork += scattered.TilesMissing;

            if (!m_Workers)
                m_Workers = std::make_unique<WorkerPool>(1);

            // A worker gets a copy of the tile and of the rules: the terrain may change meanwhile.
            const auto sharedRules = std::make_shared<const std::vector<ScatterRule>>(rules);
            for (uint32_t tile = 0; tile < scattered.Tiles.size(); ++tile)
            {
                auto copy = std::make_shared<const TerrainTileCopy>(terrain->CopyTile(tile % tilesPerSide, tile / tilesPerSide));
                m_Workers->Submit([this, entity, tile, start = scattered.Start, origin, copy, sharedRules]()
                {
                    Finished finished{ entity, start, tile, ScatterOverTile(*copy, *sharedRules, origin) };
                    std::lock_guard<std::mutex> lock(m_FinishedMutex);
                    m_Finished.push_back(std::move(finished));
                });
            }
        }

        TakeFinishedTiles(scene);
    }

    void TerrainScatterer::TakeFinishedTiles(Scene& scene)
    {
        std::vector<Finished> finished;
        {
            std::lock_guard<std::mutex> lock(m_FinishedMutex);
            finished.swap(m_Finished);
        }

        entt::registry& registry = scene.Reg();
        for (Finished& tile : finished)
        {
            const auto found = m_Scattered.find(tile.Entity);
            if (found == m_Scattered.end() || found->second.Start != tile.Start)
                continue;

            Scattered& scattered = found->second;
            scattered.Tiles[tile.Tile] = std::move(tile.Instances);
            --scattered.TilesMissing;
            --m_TilesInWork;
            if (scattered.TilesMissing > 0 || !registry.valid(tile.Entity))
                continue;

            // Complete. The tiles go in their own order, so that the list is the same every time,
            // whichever came back first.
            ScatterComponent component;
            component.Kinds = scattered.Kinds;
            for (std::vector<ScatterInstance>& instances : scattered.Tiles)
            {
                component.Instances.insert(component.Instances.end(), instances.begin(), instances.end());
                instances = {};
            }
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - scattered.StartedAt).count();
            LX_CORE_INFO("Scattered {0} instances over a terrain of {1} tiles in {2:.0f} ms.", component.Instances.size(), scattered.Tiles.size(), seconds * 1000.0);
            registry.emplace_or_replace<ScatterComponent>(tile.Entity, std::move(component));
        }
    }
}
