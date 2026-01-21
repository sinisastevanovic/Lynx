#include "MyGameModule.h"

#include <Lynx/TypeRegistry.h>

#include "Components/GameComponents.h"
#include "Systems/CameraSystem.h"
#include "Systems/EnemySystem.h"
#include "Systems/PlayerSystem.h"
#include "Systems/ProjectileSystem.h"
#include "Systems/WeaponSystem.h"

#include <imgui.h>

#include "GameConfig.h"
#include "Lynx/Utils/JsonHelpers.h"

#include "Components/TestNativeScript.h"
#include "Lynx/ImGui/LXUI.h"
#include "Systems/ExperienceSystem.h"
#include "Systems/GameManagerSystem.h"
#include "Systems/HealthSystem.h"
#include "Systems/HUDSystem.h"
#include "Systems/LifecycleSystem.h"
#include "Systems/PickupSystem.h"
#include "Systems/WaveSystem.h"

void MyGame::RegisterScripts(GameTypeRegistry* registry)
{
    registry->RegisterScript<TestNativeScript>("TestNativeScript");
}

void MyGame::RegisterComponents(GameTypeRegistry* registry)
{
    LX_INFO("Registering custom types...");
    registry->RegisterComponent<PlayerComponent>("Player Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& playerComponent = reg.get<PlayerComponent>(entity);
        json["PlayerIndex"] = playerComponent.PlayerIndex;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& playerComponent = reg.get_or_emplace<PlayerComponent>(entity);
        playerComponent.PlayerIndex = json.value("PlayerIndex", 0);
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& player = reg.get<PlayerComponent>(entity);
        LXUI::DrawDragInt("PlayerIndex", player.PlayerIndex, 1, 0, 10);
    });
    
    registry->RegisterComponent<CharacterStatsComponent>("Character Stats",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<CharacterStatsComponent>(entity);
        json["MoveSpeed"] = comp.MoveSpeed;
        json["Strength"] = comp.Strength;
        json["MagnetRadius"] = comp.MagnetRadius;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<CharacterStatsComponent>(entity);
        comp.MoveSpeed = json["MoveSpeed"];
        comp.Strength = json["Strength"];
        comp.MagnetRadius = json["MagnetRadius"];
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<CharacterStatsComponent>(entity);
        LXUI::DrawDragFloat("MoveSpeed", comp.MoveSpeed, 0.1f, 0, FLT_MAX);
        LXUI::DrawDragFloat("Strength", comp.Strength, 0.1f, 0, FLT_MAX);
        LXUI::DrawDragFloat("MagnetRadius", comp.MagnetRadius, 0.1f, 0, FLT_MAX);
    });

    registry->RegisterComponent<EnemyComponent>("Enemy Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<EnemyComponent>(entity);
        json["MoveSpeed"] = comp.MoveSpeed;
        json["XPValue"] = comp.XPValue;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<EnemyComponent>(entity);
        comp.MoveSpeed = json["MoveSpeed"];
        comp.XPValue = json["XPValue"];
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<EnemyComponent>(entity);
        LXUI::DrawDragFloat("Move Speed", comp.MoveSpeed, 0.1f, 0.0f, FLT_MAX);
        LXUI::DrawDragFloat("XPValue", comp.XPValue, 1.0f, 0.0f, FLT_MAX);
    });

    registry->RegisterComponent<WeaponComponent>("Weapon Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<WeaponComponent>(entity);
        json["FireRate"] = comp.FireRate;
        json["Range"] = comp.Range;
        json["Damage"] = comp.Damage;
        json["ProjectileSpeed"] = comp.ProjectileSpeed;
        json["ProjectilePrefab"] = comp.ProjectilePrefab;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<WeaponComponent>(entity);
        comp.FireRate = json["FireRate"];
        comp.Range = json["Range"];
        comp.Damage = json["Damage"];
        comp.ProjectileSpeed = json["ProjectileSpeed"];
        comp.ProjectilePrefab = json["ProjectilePrefab"].get<AssetHandle>();
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<WeaponComponent>(entity);
        LXUI::DrawDragFloat("Fire Rate", comp.FireRate, 0.1f);
        LXUI::DrawDragFloat("Range", comp.Range, 0.1f);
        LXUI::DrawDragFloat("Damage", comp.Damage, 0.1f);
        LXUI::DrawDragFloat("Projectile Speed", comp.ProjectileSpeed, 0.1f);
        LXUI::DrawAssetReference("Bullet Prefab", comp.ProjectilePrefab, {AssetType::Prefab});
    });

    registry->RegisterComponent<ProjectileComponent>("Projectile Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<ProjectileComponent>(entity);
        json["Damage"] = comp.Damage;
        json["Lifetime"] = comp.Lifetime;
        json["Radius"] = comp.Radius;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<ProjectileComponent>(entity);
        comp.Damage = json["Damage"];
        comp.Lifetime = json["Lifetime"];
        comp.Radius = json["Radius"];
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<ProjectileComponent>(entity);
        LXUI::DrawDragFloat("Damage", comp.Damage, 0.1f);
        LXUI::DrawDragFloat("Lifetime", comp.Lifetime, 0.1f);
        LXUI::DrawDragFloat("Radius", comp.Radius, 0.1f);
    });
    
    registry->RegisterComponent<ExperienceComponent>("Experience Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<ExperienceComponent>(entity);
        json["CurrentXP"] = comp.CurrentXP;
        json["TargetXP"] = comp.TargetXP;
        json["Level"] = comp.Level;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<ExperienceComponent>(entity);
        comp.CurrentXP = json.value("CurrentXP", 0.0f);
        comp.TargetXP = json.value("TargetXP", 100.0f);
        comp.Level = json.value("Level", 1);
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<ExperienceComponent>(entity);
        
        LXUI::DrawDragFloat("CurrentXP", comp.CurrentXP);
        LXUI::DrawDragFloat("TargetXP", comp.TargetXP);
        LXUI::DrawDragInt("Level", comp.Level, 1, 0, 9999);
    });
    
    registry->RegisterComponent<HealthComponent>("Health Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<HealthComponent>(entity);
        json["CurrHealth"] = comp.CurrentHealth;
        json["MaxHealth"] = comp.MaxHealth;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<HealthComponent>(entity);
        comp.CurrentHealth = json.value("CurrHealth", 100.0f);
        comp.MaxHealth = json.value("MaxHealth", 100.0f);
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<HealthComponent>(entity);
        
        LXUI::DrawDragFloat("CurrentHP", comp.CurrentHealth);
        LXUI::DrawDragFloat("MaxHP", comp.MaxHealth);
    });
    
    registry->RegisterComponent<PickupComponent>("Pickup Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<PickupComponent>(entity);
        json["PickupType"] = (int)comp.Type;
        json["Value"] = comp.Value;
        json["Magnetic"] = comp.Magnetic;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<PickupComponent>(entity);
        comp.Type = (PickupType)json.value("PickupType", 0);
        comp.Value = json.value("Value", 10.0f);
        comp.Magnetic = json.value("Magnetic", false);
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<PickupComponent>(entity);
        
        std::vector<std::string> typeOptions = { "XP", "Health" };
        int currType = (int)comp.Type;
        if (LXUI::DrawComboControl("Type", currType, typeOptions))
        {
            comp.Type = (PickupType)currType;
        }
        LXUI::DrawDragFloat("Value", comp.Value);
        LXUI::DrawCheckBox("Magnetic", comp.Magnetic);
    });
    
    registry->RegisterComponent<MagnetComponent>("Magnet Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<MagnetComponent>(entity);
        json["Strength"] = comp.Strength;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<MagnetComponent>(entity);
        comp.Strength = json.value("Strength", 10.0f);
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<MagnetComponent>(entity);
        
        LXUI::DrawDragFloat("Strength", comp.Strength, 0.1f, 0.0f, FLT_MAX);
    });
    
    registry->RegisterComponent<PlayerHUDComponent>("Player HUD Binding",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<PlayerHUDComponent>(entity);
        json["HPBar"] = comp.HPBar;
        json["HPText"] = comp.HPText;
        json["XPBar"] = comp.XPBar;
        json["LevelText"] = comp.LevelText;
        json["LevelUpCanavs"] = comp.LevelUpCanvas;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<PlayerHUDComponent>(entity);
        comp.HPBar = json.value("HPBar", ElementRef<UIImage>());
        comp.HPText = json.value("HPText", ElementRef<UIText>());
        comp.XPBar = json.value("XPBar", ElementRef<UIImage>());
        comp.LevelText = json.value("LevelText", ElementRef<UIText>());
        comp.LevelUpCanvas = json.value("LevelUpCanavs", ElementRef<UICanvas>());
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<PlayerHUDComponent>(entity);
        auto scene = reg.ctx().get<Scene*>();
        
        LXUI::DrawUIElementReference("HP Bar", comp.HPBar, scene);
        LXUI::DrawUIElementReference("HP Text", comp.HPText, scene);
        LXUI::DrawUIElementReference("XP Bar", comp.XPBar, scene);
        LXUI::DrawUIElementReference("Level Text", comp.LevelText, scene);
        LXUI::DrawUIElementReference("LevelUpCanvas", comp.LevelUpCanvas, scene);
    });
    
    registry->RegisterComponent<WaveManagerComponent>("Wave Manager",
   [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
       {
           auto& comp = reg.get<WaveManagerComponent>(entity);
           json["TimerTextElement"] = comp.GameTimerText;
       },
       [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
       {
           auto& comp = reg.get_or_emplace<WaveManagerComponent>(entity);
           comp.GameTimerText = json.value("TimerTextElement", ElementRef<UIText>());
       },
       [](entt::registry& reg, entt::entity entity)
       {
           auto& comp = reg.get<WaveManagerComponent>(entity);
           auto scene = reg.ctx().get<Scene*>();
            
           LXUI::DrawUIElementReference("Timer Text", comp.GameTimerText, scene);
       });
}

void MyGame::OnStart()
{
    LX_INFO("OnStart called.");

    auto scene = Engine().Get().GetActiveScene();
    
    GameConfig::LoadAssets();
    
    GameManagerSystem::Init(scene);
    
    DamageTextSystem::Init(scene);
    
    Input::BindAxis("MoveLeftRight", KeyCode::D, KeyCode::A);
    Input::BindAxis("MoveUpDown", KeyCode::S, KeyCode::W);
    
    auto gameManger = scene->CreateEntity("GameManager");
    gameManger.AddComponent<WaveManagerComponent>();
    
    WaveSystem::Init();
}

void MyGame::OnUpdate(float deltaTime)
{
    auto scene = Engine::Get().GetActiveScene();
    if (!scene) return;
    
    // 1. Spawning & Movement   
    WaveSystem::Update(scene, deltaTime);
    EnemySystem::Update(scene, deltaTime);
    PlayerSystem::Update(scene, deltaTime);
    
    // 2. Combat & Action
    WeaponSystem::Update(scene, deltaTime);
    ProjectileSystem::Update(scene, deltaTime);
    PickupSystem::Update(scene, deltaTime);
    
    // 3. Rules & Logic
    HealthSystem::Update(scene);
    ExperienceSystem::Update(scene, deltaTime);
    
    // 4. Cleanup & Attribution
    LifecycleSystem::Update(scene);
    
    // 5. Presentation (Visuals)
    CameraSystem::Update(scene, deltaTime);
    HUDSystem::Update(scene, deltaTime);
    DamageTextSystem::Update(deltaTime, scene);
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
    GameConfig::UnloadAssets();
    DamageTextSystem::Shutdown();
   // Engine::Get().GetAssetManager().UnloadAsset(m_ParticleMat->GetHandle()); // TODO: This is temporary!! We need a way to clear all runtime game assets. 
}