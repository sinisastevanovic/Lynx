#include "MyGameModule.h"

#include <Lynx/ComponentRegistry.h>
#include <Lynx/Scene/Scene.h>
#include <Lynx/Scene/Components/Components.h>
#include <Lynx/Scene/Entity.h>
#include <Lynx/Engine.h>

#include "Lynx/Scene/Components/PhysicsComponents.h"

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
}

void MyGame::OnUpdate(float deltaTime)
{
    /*auto scene = Lynx::Engine::Get().GetActiveScene();
    // Example: Rotate the cube
    auto view = scene->Reg().view<Lynx::TransformComponent, Lynx::MeshComponent>();
    for (auto entity : view)
    {
        auto& transform = view.get<Lynx::TransformComponent>(entity);
        glm::quat deltaRotY = glm::angleAxis(deltaTime * 1.0f, glm::vec3(0, 1, 0));
        glm::quat deltaRotX = glm::angleAxis(deltaTime * 0.5f, glm::vec3(1, 0, 0));

        transform.Rotation = transform.Rotation * deltaRotY * deltaRotX;
    }*/
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
}