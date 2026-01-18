#include "MyGameModule.h"

#include <Lynx/TypeRegistry.h>

#include "Components/GameComponents.h"
#include "Systems/CameraSystem.h"
#include "Systems/EnemySystem.h"
#include "Systems/PlayerSystem.h"
#include "Systems/ProjectileSystem.h"
#include "Systems/WeaponSystem.h"

#include <imgui.h>
#include "Lynx/Utils/JsonHelpers.h"

#include "Components/TestNativeScript.h"
#include "Lynx/ImGui/LXUI.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"
#include "Systems/ExperienceSystem.h"
#include "Systems/HUDSystem.h"

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
        json["MoveSpeed"] = playerComponent.MoveSpeed;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& playerComponent = reg.get_or_emplace<PlayerComponent>(entity);
        playerComponent.MoveSpeed = json["MoveSpeed"];
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& player = reg.get<PlayerComponent>(entity);
        LXUI::DrawDragFloat("Move Speed", player.MoveSpeed, 0.1f);
    });

    registry->RegisterComponent<EnemyComponent>("Enemy Component",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<EnemyComponent>(entity);
        json["MoveSpeed"] = comp.MoveSpeed;
        json["Health"] = comp.Health;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<EnemyComponent>(entity);
        comp.MoveSpeed = json["MoveSpeed"];
        comp.Health = json["Health"];
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<EnemyComponent>(entity);
        LXUI::DrawDragFloat("Move Speed", comp.MoveSpeed, 0.1f);
        LXUI::DrawDragFloat("Health", comp.Health, 0.1f);
    });

    registry->RegisterComponent<EnemySpawnerComponent>("EnemySpawnerComponent",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<EnemySpawnerComponent>(entity);
        json["SpawnRate"] = comp.SpawnRate;
        json["MaxEnemies"] = comp.MaxEnemies;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<EnemySpawnerComponent>(entity);
        comp.SpawnRate = json["SpawnRate"];
        comp.MaxEnemies = json["MaxEnemies"];
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<EnemySpawnerComponent>(entity);
        LXUI::DrawDragFloat("SpawnRate", comp.SpawnRate, 0.1f);
        LXUI::DrawDragInt("MaxEnemies", comp.MaxEnemies, 1);
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
    
    registry->RegisterComponent<PlayerHUDComponent>("Player HUD Binding",
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<PlayerHUDComponent>(entity);
        json["HPBar"] = comp.HPBarID;
        json["HPText"] = comp.HPTextID;
        json["XPBar"] = comp.XPBarID;
        json["LevelText"] = comp.LevelTextID;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<PlayerHUDComponent>(entity);
        comp.HPBarID = json.value<UUID>("HPBar", UUID::Null());
        comp.HPTextID = json.value<UUID>("HPText", UUID::Null());
        comp.XPBarID = json.value<UUID>("XPBar", UUID::Null());
        comp.LevelTextID = json.value<UUID>("LevelText", UUID::Null());
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<PlayerHUDComponent>(entity);
        auto scene = reg.ctx().get<Scene*>();
        
        LXUI::DrawUIElementSelection("HP Bar", comp.HPBarID, scene);
        LXUI::DrawUIElementSelection("HP Text", comp.HPTextID, scene);
        LXUI::DrawUIElementSelection("XP Bar", comp.XPBarID, scene);
        LXUI::DrawUIElementSelection("Level Text", comp.LevelTextID, scene);
    });
}

void MyGame::OnStart()
{
    LX_INFO("OnStart called.");

    auto scene = Engine().Get().GetActiveScene();
    
    DamageTextSystem::Init(scene);

    Input::BindAxis("MoveLeftRight", KeyCode::D, KeyCode::A);
    Input::BindAxis("MoveUpDown", KeyCode::S, KeyCode::W);
}

void MyGame::OnUpdate(float deltaTime)
{
    auto scene = Engine::Get().GetActiveScene();
    
    EnemySystem::Update(scene, deltaTime);
    PlayerSystem::Update(scene, deltaTime);
    CameraSystem::Update(scene, deltaTime);

    ProjectileSystem::Update(scene, deltaTime);
    WeaponSystem::Update(scene, deltaTime);
    DamageTextSystem::Update(deltaTime, scene);
    ExperienceSystem::Update(scene, deltaTime);
    HUDSystem::Update(scene, deltaTime);
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
    DamageTextSystem::Shutdown();
   // Engine::Get().GetAssetManager().UnloadAsset(m_ParticleMat->GetHandle()); // TODO: This is temporary!! We need a way to clear all runtime game assets. 
}