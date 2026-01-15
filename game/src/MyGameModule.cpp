#include "MyGameModule.h"

#include <Lynx/TypeRegistry.h>
#include <Lynx/Scene/Scene.h>
#include <Lynx/Scene/Components/Components.h>
#include <Lynx/Scene/Entity.h>
#include <Lynx/Engine.h>

#include "Components/GameComponents.h"
#include "Lynx/Input.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"
#include "Systems/CameraSystem.h"
#include "Systems/EnemySystem.h"
#include "Systems/PlayerSystem.h"
#include "Systems/ProjectileSystem.h"
#include "Systems/WeaponSystem.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include "Lynx/Utils/JsonHelpers.h"

#include "Components/TestNativeScript.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/ImGui/LXUI.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"
#include "Lynx/Scene/Components/NativeScriptComponent.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/UI/Core/UIElement.h"
#include "Lynx/UI/Widgets/StackPanel.h"
#include "Lynx/UI/Widgets/UIButton.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIText.h"

void MyGame::RegisterScripts(Lynx::GameTypeRegistry* registry)
{
    registry->RegisterScript<TestNativeScript>("TestNativeScript");
}

void MyGame::RegisterComponents(Lynx::GameTypeRegistry* registry)
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
        Lynx::LXUI::DrawDragFloat("Move Speed", player.MoveSpeed, 0.1f);
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
        Lynx::LXUI::DrawDragFloat("Move Speed", comp.MoveSpeed, 0.1f);
        Lynx::LXUI::DrawDragFloat("Health", comp.Health, 0.1f);
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
        Lynx::LXUI::DrawDragFloat("SpawnRate", comp.SpawnRate, 0.1f);
        Lynx::LXUI::DrawDragInt("MaxEnemies", comp.MaxEnemies, 1);
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
        comp.ProjectilePrefab = json["ProjectilePrefab"].get<Lynx::AssetHandle>();
    },
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<WeaponComponent>(entity);
        Lynx::LXUI::DrawDragFloat("Fire Rate", comp.FireRate, 0.1f);
        Lynx::LXUI::DrawDragFloat("Range", comp.Range, 0.1f);
        Lynx::LXUI::DrawDragFloat("Damage", comp.Damage, 0.1f);
        Lynx::LXUI::DrawDragFloat("Projectile Speed", comp.ProjectileSpeed, 0.1f);
        Lynx::LXUI::DrawAssetReference("Bullet Prefab", comp.ProjectilePrefab, {Lynx::AssetType::Prefab});
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
        Lynx::LXUI::DrawDragFloat("Damage", comp.Damage, 0.1f);
        Lynx::LXUI::DrawDragFloat("Lifetime", comp.Lifetime, 0.1f);
        Lynx::LXUI::DrawDragFloat("Radius", comp.Radius, 0.1f);
    });
}

void CreateMyUI(std::shared_ptr<Lynx::Scene> scene)
{
    using namespace Lynx;
    Entity uiEntity = scene->CreateEntity("MainMenu");
    auto& canvasComp = uiEntity.AddComponent<UICanvasComponent>();
    return;
    auto root = canvasComp.Canvas;
    
    // 1. Fullscreen Background
    auto bg = std::make_shared<UIImage>();
    bg->SetName("Background");
    bg->SetAnchor(UIAnchor::StretchAll);
    bg->SetTint({ 0.1f, 0.1f, 0.2f, 1.0f }); // Dark Blue
    root->AddChild(bg);
    
    // 2. Center Panel (The Menu Window)
    auto window = std::make_shared<UIImage>();
    window->SetName("MenuWindow");
    window->SetAnchor(UIAnchor::Center);
    window->SetSize({ 400.0f, 500.0f }); // Fixed size window
    window->SetTint({ 0.2f, 0.2f, 0.2f, 0.9f }); // Dark Grey
    
    // window->SetType(ImageType::Sliced);
    // window->SetBorder({5,5,5,5});
    
    bg->AddChild(window);
    
    // 3. StackPanel (The Layout Manager)
    auto stack = std::make_shared<StackPanel>();
    stack->SetName("ButtonStack");
    stack->SetAnchor(UIAnchor::StretchAll); // Fill the window
    stack->SetOffset({ 0.0f, 0.0f }); // Padding 20dp
    stack->SetSize({ 0.0f, 0.0f }); // Negative size delta = margins from Right/Bottom
    
    stack->SetOrientation(Orientation::Vertical);
    stack->SetSpacing(15.0f); // 15dp gap between buttons
    window->AddChild(stack);
    
    // 4. Test Buttons
    
    // A. Stretch Button (Default)
    auto btnStretch = std::make_shared<UIButton>();
    btnStretch->SetName("Btn_Stretch");
    btnStretch->SetSize({ 0.0f, 50.0f }); // Width ignored in stretch, Height 50
    btnStretch->SetTint({ 0.2f, 0.6f, 0.2f, 1.0f }); // Green
    btnStretch->SetHorizontalAlignment(UIAlignment::Stretch);
    stack->AddChild(btnStretch);

    auto btnText = std::make_shared<UIText>();
    btnText->SetName("Btn_Text");
    btnText->SetSize({ 0.0f, 0.0f });
    btnText->SetAnchor(UIAnchor::StretchAll);
    btnStretch->AddChild(btnText);
    
    // B. Left/Start Button
    auto btnLeft = std::make_shared<UIButton>();
    btnLeft->SetName("Btn_Left");
    btnLeft->SetSize({ 150.0f, 50.0f }); // Fixed width
    btnLeft->SetTint({ 0.6f, 0.2f, 0.2f, 1.0f }); // Red
    btnLeft->SetHorizontalAlignment(UIAlignment::Start);
    stack->AddChild(btnLeft);
    
    // C. Center Button
    auto btnCenter = std::make_shared<UIButton>();
    btnCenter->SetName("Btn_Center");
    btnCenter->SetSize({ 150.0f, 50.0f });
    btnCenter->SetTint({ 0.2f, 0.2f, 0.6f, 1.0f }); // Blue
    btnCenter->SetHorizontalAlignment(UIAlignment::Center);
    stack->AddChild(btnCenter);
    
    // D. Right/End Button
    auto btnRight = std::make_shared<UIButton>();
    btnRight->SetName("Btn_Right");
    btnRight->SetSize({ 150.0f, 50.0f });
    btnRight->SetTint({ 0.6f, 0.6f, 0.2f, 1.0f }); // Yellow
    btnRight->SetHorizontalAlignment(UIAlignment::End);
    stack->AddChild(btnRight);
}

void MyGame::OnStart()
{
    LX_INFO("OnStart called.");

    auto scene = Lynx::Engine().Get().GetActiveScene();
    
    DamageTextSystem::Init(scene);

    Lynx::Input::BindAxis("MoveLeftRight", Lynx::KeyCode::D, Lynx::KeyCode::A);
    Lynx::Input::BindAxis("MoveUpDown", Lynx::KeyCode::S, Lynx::KeyCode::W);
}

void MyGame::OnUpdate(float deltaTime)
{
    auto scene = Lynx::Engine::Get().GetActiveScene();
    
    EnemySystem::Update(scene, deltaTime);
    PlayerSystem::Update(scene, deltaTime);
    CameraSystem::Update(scene, deltaTime);

    ProjectileSystem::Update(scene, deltaTime);
    WeaponSystem::Update(scene, deltaTime);
    DamageTextSystem::Update(deltaTime, scene);
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
    DamageTextSystem::Shutdown();
   // Lynx::Engine::Get().GetAssetManager().UnloadAsset(m_ParticleMat->GetHandle()); // TODO: This is temporary!! We need a way to clear all runtime game assets. 
}