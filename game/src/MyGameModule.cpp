#include "MyGameModule.h"

#include <Lynx/ComponentRegistry.h>
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

#include "Components/TestNativeScript.h"
#include "Lynx/ScriptRegistry.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"
#include "Lynx/Scene/Components/NativeScriptComponent.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/UI/Core/UIElement.h"
#include "Lynx/UI/Widgets/StackPanel.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIText.h"

void MyGame::RegisterScripts()
{
    Lynx::ScriptRegistry::RegisterScript<TestNativeScript>("TestNativeScript");
}

void MyGame::RegisterComponents(Lynx::ComponentRegistry* registry)
{
    LX_INFO("Registering custom types...");
    registry->RegisterComponent<PlayerComponent>("Player Component",
    [](entt::registry& reg, entt::entity entity)
    {
        auto& player = reg.get<PlayerComponent>(entity);
        ImGui::DragFloat("Move Speed", &player.MoveSpeed, 0.1f);
    },
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& playerComponent = reg.get<PlayerComponent>(entity);
        json["MoveSpeed"] = playerComponent.MoveSpeed;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& playerComponent = reg.get_or_emplace<PlayerComponent>(entity);
        playerComponent.MoveSpeed = json["MoveSpeed"];
    });

    registry->RegisterComponent<EnemyComponent>("Enemy Component",
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<EnemyComponent>(entity);
        ImGui::DragFloat("Move Speed", &comp.MoveSpeed, 0.1f);
        ImGui::DragFloat("Health", &comp.Health, 0.1f);
    },
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
    });

    registry->RegisterComponent<EnemySpawnerComponent>("EnemySpawnerComponent",
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<EnemySpawnerComponent>(entity);
        ImGui::DragFloat("SpawnRate", &comp.SpawnRate, 0.1f);
        ImGui::DragInt("MaxEnemies", &comp.MaxEnemies, 1);
    },
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
    });

    registry->RegisterComponent<WeaponComponent>("Weapon Component",
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<WeaponComponent>(entity);
        ImGui::DragFloat("Fire Rate", &comp.FireRate, 0.1f);
        ImGui::DragFloat("Range", &comp.Range, 0.1f);
        ImGui::DragFloat("Damage", &comp.Damage, 0.1f);
        ImGui::DragFloat("Projectile Speed", &comp.ProjectileSpeed, 0.1f);
    },
    [](entt::registry& reg, entt::entity entity, nlohmann::json& json)
    {
        auto& comp = reg.get<WeaponComponent>(entity);
        json["FireRate"] = comp.FireRate;
        json["Range"] = comp.Range;
        json["Damage"] = comp.Damage;
        json["ProjectileSpeed"] = comp.ProjectileSpeed;
    },
    [](entt::registry& reg, entt::entity entity, const nlohmann::json& json)
    {
        auto& comp = reg.get_or_emplace<WeaponComponent>(entity);
        comp.FireRate = json["FireRate"];
        comp.Range = json["Range"];
        comp.Damage = json["Damage"];
        comp.ProjectileSpeed = json["ProjectileSpeed"];
    });

    registry->RegisterComponent<ProjectileComponent>("Projectile Component",
    [](entt::registry& reg, entt::entity entity)
    {
        auto& comp = reg.get<ProjectileComponent>(entity);
        ImGui::DragFloat("Damage", &comp.Damage, 0.1f);
        ImGui::DragFloat("Lifetime", &comp.Lifetime, 0.1f);
        ImGui::DragFloat("Radius", &comp.Radius, 0.1f);
    },
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
    });
}

void CreateMyUI(std::shared_ptr<Lynx::Scene> scene)
{
    using namespace Lynx;
    Entity uiEntity = scene->CreateEntity("MainMenu");
    auto& canvasComp = uiEntity.AddComponent<UICanvasComponent>();
    auto root = canvasComp.Canvas;
    
    // 1. Fullscreen Background
    auto bg = std::make_shared<UIImage>();
    bg->SetName("Background");
    bg->SetAnchor(UIAnchor::StretchAll);
    bg->SetColor({ 0.1f, 0.1f, 0.2f, 1.0f }); // Dark Blue
    root->AddChild(bg);
    
    // 2. Center Panel (The Menu Window)
    auto window = std::make_shared<UIImage>();
    window->SetName("MenuWindow");
    window->SetAnchor(UIAnchor::Center);
    window->SetSize({ 400.0f, 500.0f }); // Fixed size window
    window->SetColor({ 0.2f, 0.2f, 0.2f, 0.9f }); // Dark Grey
    
    // Optional: Give it a border if you have a 9-slice sprite
    // window->SetType(ImageType::Sliced);
    // window->SetBorder({5,5,5,5});
    
    bg->AddChild(window);
    
    // 3. StackPanel (The Layout Manager)
    auto stack = std::make_shared<StackPanel>();
    stack->SetName("ButtonStack");
    stack->SetAnchor(UIAnchor::StretchAll); // Fill the window
    stack->SetOffset({ 0.0f, 0.0f }); // Padding 20dp
    stack->SetSize({ 0.0f, 0.0f }); // Negative size delta = margins from Right/Bottom
    // Note: If your Stretch logic uses Size as Delta, use negative.
    // If your logic adds Size to Width, you need CalculateBounds to handle "Margin via Size".
    // Alternatively: Set Anchor Stretch and Offset 20, 20 and Size 0?
    // Usually: Left=20, Top=20, Right=20, Bottom=20 requires offsets on all sides.
    // Let's assume standard behavior: Size is ignored in Stretch? Or Size is Delta.
    
    // Simpler V1 approach for padding:
    // Anchor Stretch, Offset 0, Size 0 -> Full fill.
    // We rely on StackPanel's content centering?
    // Let's just pin the stack to Top-Center with a width.
    //stack->SetAnchor(UIAnchor::TopLeft); // Top Left of Window
    //stack->SetSize({ 360.0f, 460.0f }); // 400 - 40 padding
    //stack->SetOffset({ 20.0f, 20.0f });
    //stack->SetPivot({0, 0});
    
    stack->SetOrientation(Orientation::Vertical);
    stack->SetSpacing(15.0f); // 15dp gap between buttons
    window->AddChild(stack);
    
    // 4. Test Buttons
    
    // A. Stretch Button (Default)
    auto btnStretch = std::make_shared<UIImage>();
    btnStretch->SetName("Btn_Stretch");
    btnStretch->SetSize({ 0.0f, 50.0f }); // Width ignored in stretch, Height 50
    btnStretch->SetColor({ 0.2f, 0.6f, 0.2f, 1.0f }); // Green
    btnStretch->SetHorizontalAlignment(UIAlignment::Stretch);
    stack->AddChild(btnStretch);

    auto btnText = std::make_shared<UIText>();
    btnText->SetName("Btn_Text");
    btnStretch->AddChild(btnText);
    
    // B. Left/Start Button
    auto btnLeft = std::make_shared<UIImage>();
    btnLeft->SetName("Btn_Left");
    btnLeft->SetSize({ 150.0f, 50.0f }); // Fixed width
    btnLeft->SetColor({ 0.6f, 0.2f, 0.2f, 1.0f }); // Red
    btnLeft->SetHorizontalAlignment(UIAlignment::Start);
    stack->AddChild(btnLeft);
    
    // C. Center Button
    auto btnCenter = std::make_shared<UIImage>();
    btnCenter->SetName("Btn_Center");
    btnCenter->SetSize({ 150.0f, 50.0f });
    btnCenter->SetColor({ 0.2f, 0.2f, 0.6f, 1.0f }); // Blue
    btnCenter->SetHorizontalAlignment(UIAlignment::Center);
    stack->AddChild(btnCenter);
    
    // D. Right/End Button
    auto btnRight = std::make_shared<UIImage>();
    btnRight->SetName("Btn_Right");
    btnRight->SetSize({ 150.0f, 50.0f });
    btnRight->SetColor({ 0.6f, 0.6f, 0.2f, 1.0f }); // Yellow
    btnRight->SetHorizontalAlignment(UIAlignment::End);
    stack->AddChild(btnRight);
}

void MyGame::OnStart()
{
    LX_INFO("OnStart called.");

    // Load Texture
    auto& engine = Lynx::Engine::Get();
    auto& assetManager = engine.GetAssetManager();
    auto scene = engine.GetActiveScene();

    auto mesh = assetManager.GetAssetHandle("assets/Models/Bottle/WaterBottle.gltf");
    //auto floorMesh = assetManager.GetAssetHandle("assets/Models/Cube/Cube.gltf");
    auto floorMesh = assetManager.GetDefaultCube();
    auto playerMesh = assetManager.GetAssetHandle("assets/Models/Fox/Fox.gltf");

    {
        auto camera = scene->CreateEntity("Camera");
        auto& transform = camera.GetComponent<Lynx::TransformComponent>();
        transform.Translation = { 0.0f, 15.0f, 10.0f };
        transform.Rotation = glm::angleAxis(glm::radians(-60.0f), glm::vec3(1, 0, 0));
        auto& cam = camera.AddComponent<Lynx::CameraComponent>();
        cam.Primary = true;
    }

    {
        auto player = scene->CreateEntity("Player");
        auto& transform = player.GetComponent<Lynx::TransformComponent>();
        transform.Translation = { 0.0f, 2.0f, 0.0f };
        transform.Scale = { 0.01f, 0.01f, 0.01f };

        auto& meshComp = player.AddComponent<Lynx::MeshComponent>();
        meshComp.Mesh = playerMesh;

        player.AddComponent<PlayerComponent>();

        auto& rb = player.AddComponent<Lynx::RigidBodyComponent>();
        rb.Type = Lynx::RigidBodyType::Dynamic;
        rb.LockAllRotation();

        auto& collider = player.AddComponent<Lynx::CapsuleColliderComponent>();
        collider.Radius = 0.5f;
        collider.HalfHeight = 1.0f;

        player.AddComponent<WeaponComponent>();
    }

    {
        auto player = scene->CreateEntity("Test");
        auto& transform = player.GetComponent<Lynx::TransformComponent>();
        transform.Translation = { 0.0f, 2.0f, 0.0f };
        transform.Scale = { 0.01f, 0.01f, 0.01f };

        auto& meshComp = player.AddComponent<Lynx::MeshComponent>();
        meshComp.Mesh = playerMesh;

        //player.AddComponent<Lynx::NativeScriptComponent>().Bind<TestNativeScript>();
        player.AddComponent<Lynx::LuaScriptComponent>().ScriptHandle = assetManager.GetAssetHandle("assets/Scripts/test.lua");
    }

    {
        auto floor = scene->CreateEntity("Floor");
        auto& transform = floor.GetComponent<Lynx::TransformComponent>();
        transform.Translation = { 0.0f, -2.0f, 0.0f };
        transform.Scale = { 100.0f, 1.0f, 100.0f };

        auto& meshComp = floor.AddComponent<Lynx::MeshComponent>();
        meshComp.Mesh = floorMesh->GetHandle();

        auto& rb = floor.AddComponent<Lynx::RigidBodyComponent>();
        rb.Type = Lynx::RigidBodyType::Static;

        auto& collider = floor.AddComponent<Lynx::BoxColliderComponent>();
        collider.HalfSize = { 50.0f, 0.5f, 50.0f };
    }

    {
        auto bottle = scene->CreateEntity("Bottle");
        auto& transform = bottle.GetComponent<Lynx::TransformComponent>();
        transform.Translation = { 0.0f, 10.0f, 0.0f };
        transform.SetRotationEuler({1, 1, 0});
        transform.Scale = { 2.0f, 2.0f, 2.0f };

        auto& meshComp = bottle.AddComponent<Lynx::MeshComponent>();
        meshComp.Mesh = mesh;

        auto& rb = bottle.AddComponent<Lynx::RigidBodyComponent>();
        rb.Type = Lynx::RigidBodyType::Dynamic;

        auto& collider = bottle.AddComponent<Lynx::BoxColliderComponent>();
        collider.HalfSize = { 0.1f, 0.3f, 0.1f };
    }

    {
        auto fireEntity = scene->CreateEntity("Campfire Particle");

        auto& transform = fireEntity.GetComponent<Lynx::TransformComponent>();
        transform.Translation = { 0.0f, 1.0f, 0.0f };

        auto& emitter = fireEntity.AddComponent<Lynx::ParticleEmitterComponent>();

        // --- Configuration for FIRE Effect ---
        emitter.EmissionRate = 50.0f;   
        emitter.Properties.LifeTime = 1.5f; 
        emitter.Properties.Position = { 0.0f, 0.0f, 0.0f };

        emitter.Properties.Velocity = { 0.0f, 2.0f, 0.0f };
        emitter.Properties.VelocityVariation = { 0.5f, 1.0f, 0.5f };

        // Color: Bright Orange -> Fading Red
        emitter.Properties.ColorBegin = { 254.0f/255.0f, 212.0f/255.0f, 123.0f/255.0f, 1.0f };
        emitter.Properties.ColorEnd   = { 254.0f/255.0f, 109.0f/255.0f, 41.0f/255.0f, 0.0f };

        // Size: Big base -> Small tip
        emitter.Properties.SizeBegin = 0.5f;
        emitter.Properties.SizeEnd = 0.1f;
        emitter.Properties.SizeVariation = 0.2f;

        auto particleMat = std::make_shared<Lynx::Material>();
        particleMat->AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f }; 
        particleMat->Mode = Lynx::AlphaMode::Additive;
        particleMat->UseNormalMap = false;

        Lynx::Engine::Get().GetAssetManager().AddRuntimeAsset(particleMat);

        emitter.Material = particleMat->GetHandle();
    }

    auto spawnerEntity = scene->CreateEntity("Spawner");
    auto& spawner = spawnerEntity.AddComponent<EnemySpawnerComponent>();

    CreateMyUI(scene);

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
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
}