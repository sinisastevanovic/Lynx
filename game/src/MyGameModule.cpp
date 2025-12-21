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

void MyGame::RegisterComponents(Lynx::ComponentRegistry* registry)
{
    LX_INFO("Registering custom types...");
    registry->RegisterComponent<PlayerComponent>("Player Component");
}

void MyGame::OnStart()
{
    LX_INFO("OnStart called.");

    // Load Texture
    auto& engine = Lynx::Engine::Get();
    auto& assetManager = engine.GetAssetManager();
    auto scene = engine.GetActiveScene();

    auto mesh = assetManager.GetMesh("assets/WaterBottle/glTF/WaterBottle.gltf");
    //auto floorMesh = assetManager.GetMesh("assets/Cube/Cube.gltf");
    auto floorMesh = assetManager.GetDefaultCube();
    auto playerMesh = assetManager.GetMesh("assets/Fox/glTF/Fox.gltf");

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
        meshComp.Mesh = playerMesh->GetHandle();

        player.AddComponent<PlayerComponent>();

        auto& rb = player.AddComponent<Lynx::RigidBodyComponent>();
        rb.Type = Lynx::RigidBodyType::Dynamic;
        rb.LockAllRotation();

        auto& collider = player.AddComponent<Lynx::CapsuleColliderComponent>();
        collider.Radius = 0.5f;
        collider.Height = 2.0f;

        player.AddComponent<WeaponComponent>();
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
        meshComp.Mesh = mesh->GetHandle();

        auto& rb = bottle.AddComponent<Lynx::RigidBodyComponent>();
        rb.Type = Lynx::RigidBodyType::Dynamic;

        auto& collider = bottle.AddComponent<Lynx::BoxColliderComponent>();
        collider.HalfSize = { 0.1f, 0.3f, 0.1f };
    }

    auto spawnerEntity = scene->CreateEntity("Spawner");
    auto& spawner = spawnerEntity.AddComponent<EnemySpawnerComponent>();
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