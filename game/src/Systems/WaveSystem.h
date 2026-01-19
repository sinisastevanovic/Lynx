#pragma once

#include "Lynx.h"
#include "Components/GameComponents.h"
#include "Lynx/Asset/Prefab.h"

struct WaveDefinition
{
    float StartTime;
    float EndTime;
    float SpawnInterval;
    std::shared_ptr<Prefab> EnemyPrefab;
};

struct WaveManagerComponent
{
    ElementRef<UIText> GameTimerText;
    float GameTimer = 0.0f;
    std::unordered_map<int, float> WaveSpawnTimers;
};

class WaveSystem
{
public:
    static inline std::vector<WaveDefinition> Waves;
    
    static void Init()
    {
        auto& am = Engine::Get().GetAssetManager();
        
        auto small = am.GetAsset<Prefab>("assets/Prefabs/Enemy_Small.lxprefab");
        auto big = am.GetAsset<Prefab>("assets/Prefabs/Enemy_Big.lxprefab");
        
        Waves = {
            { 0.0f, 30.0f, 2.0f, small },
            { 10.0f, 40.0f, 5.0f, big },
            { 30.0f, 999.0f, 0.5f, small }
        };
    }
    
    static void Update(std::shared_ptr<Scene> scene, float deltaTime)
    {
        auto view = scene->Reg().view<WaveManagerComponent>();
        if (view.empty())
            return;
        
        auto& mgr = view.get<WaveManagerComponent>(view.front());
        mgr.GameTimer += deltaTime;
        
        for (int i = 0; i < Waves.size(); ++i)
        {
            const auto& wave = Waves[i];
            
            if (mgr.GameTimer >= wave.StartTime && mgr.GameTimer < wave.EndTime)
            {
                float& timer = mgr.WaveSpawnTimers[i];
                timer += deltaTime;
                
                if (timer >= wave.SpawnInterval)
                {
                    SpawnEnemy(scene, wave.EnemyPrefab);
                    timer = 0.0f;
                }
            }
        }
    }
    
    static void SpawnEnemy(std::shared_ptr<Scene> scene, std::shared_ptr<Prefab> prefab)
    {
        if (!prefab || !prefab->GetHandle().IsValid())
            return;
        
        float angle = (float)rand() / RAND_MAX * 6.28f;
        float radius = 20.0f;
        glm::vec3 offset = { cos(angle) * radius, 0.0f, sin(angle) * radius };
        
        // TODO: dont query player every frame...
        glm::vec3 center = { 0, 0, 0 };
        auto playerView = scene->Reg().view<PlayerComponent, TransformComponent>();
        for (auto entity : playerView)
        {
            auto [player, trans] = playerView.get<PlayerComponent, TransformComponent>(entity);
            center = trans.GetWorldTranslation();
            break;
        }
        
        glm::vec3 spawnPos = center + offset;
        spawnPos.y = 1.0f;
        
        auto enemy = scene->InstantiatePrefab(prefab);
        if (enemy)
        {
            enemy.GetComponent<TransformComponent>().Translation = spawnPos;
        }
    }
};