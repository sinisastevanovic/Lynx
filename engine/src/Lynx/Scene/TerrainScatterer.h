#pragma once
#include "Lynx/Core.h"
#include "Scatter.h"
#include "Lynx/Asset/TerrainScatter.h"
#include "Lynx/UUID.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Lynx
{
    class Scene;
    class WorkerPool;

    // The instances that the rules scatter over one tile of a terrain, the same every time.
    //
    // After "GPU-Based Run-Time Procedural Placement in Horizon Zero Dawn" (van Muijden, GDC 2017):
    // a rule never places a thing, it says how dense its kind stands at a place. The places come from
    // a fixed pattern of points that is anchored in the terrain, one point in every square of a grid
    // as wide as the rule's spacing, and every point has a threshold of its own. A point becomes an
    // instance where the density is above its threshold. Rules with the same spacing share the points
    // and lie on top of each other between 0 and 1, in the order of the list, so that a point falls
    // to one of them at most.
    //
    // rules are those of the terrain, and a rule's number in the list is the kind of its instances.
    // origin is where the terrain's first corner lies in the world.
    LX_API std::vector<ScatterInstance> ScatterOverTile(const TerrainTileCopy& tile, const std::vector<ScatterRule>& rules, const glm::vec3& origin);

    // Keeps the terrains of a scene scattered: every entity with a terrain whose asset has rules gets
    // a ScatterComponent with what the rules place, made anew when the terrain, its rules or its
    // position change. The placing runs on a thread of its own, tile by tile, and the instances there
    // were stay until the new ones are complete.
    //
    // Call Update once a frame, in the editor as while playing: scattered things are part of how a
    // terrain looks, not of the game.
    class LX_API TerrainScatterer
    {
    public:
        TerrainScatterer();
        ~TerrainScatterer();

        void Update(Scene& scene);

        // True while tiles are being scattered, so the scene does not show yet what it is going to.
        bool IsScattering() const { return m_TilesInWork > 0; }

    private:
        // What an entity's instances were made from, and the tiles that are back so far.
        struct Scattered
        {
            AssetHandle Terrain = AssetHandle::Null();
            uint32_t Version = 0;
            uint32_t RulesVersion = 0;
            glm::vec3 Origin{ 0.0f };

            // Raised with every new start, so that tiles of an earlier one are told apart and dropped.
            uint64_t Start = 0;
            std::vector<std::shared_ptr<StaticMesh>> Kinds;
            std::vector<std::vector<ScatterInstance>> Tiles;
            uint32_t TilesMissing = 0;
            std::chrono::steady_clock::time_point StartedAt;
        };

        // One tile that a worker finished.
        struct Finished
        {
            entt::entity Entity;
            uint64_t Start;
            uint32_t Tile;
            std::vector<ScatterInstance> Instances;
        };

        void TakeFinishedTiles(Scene& scene);

    private:
        std::unordered_map<entt::entity, Scattered> m_Scattered;
        uint64_t m_Starts = 0;
        uint32_t m_TilesInWork = 0;

        std::mutex m_FinishedMutex;
        std::vector<Finished> m_Finished;

        // Made with the first tile, so that a scene without scattered terrain has no thread for it.
        // Declared last: its jobs write to what stands above.
        std::unique_ptr<WorkerPool> m_Workers;
    };
}
