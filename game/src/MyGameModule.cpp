#include "MyGameModule.h"

#include <Lynx/ComponentRegistry.h>
#include <Lynx/Scene/Scene.h>
#include <Lynx/Scene/Components.h>
#include <Lynx/Scene/Entity.h>
#include <Lynx/Engine.h>

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
    
    // Create a Cube Entity
    auto scene = engine.GetActiveScene();
    auto entity = scene->CreateEntity("Cube");
    entity.AddComponent<Lynx::MeshComponent>();
    entity.GetComponent<Lynx::TransformComponent>().Translation.y = 0.5f;

    auto entity2 = scene->CreateEntity("Cube2");
    entity2.AddComponent<Lynx::MeshComponent>();
    entity2.GetComponent<Lynx::TransformComponent>().Translation.x = 0.5f;
    // Position it slightly away so we see it
    // Default transform is 0,0,0.
}

void MyGame::OnUpdate(float deltaTime)
{
    auto scene = Lynx::Engine::Get().GetActiveScene();
    // Example: Rotate the cube
    auto view = scene->Reg().view<Lynx::TransformComponent, Lynx::MeshComponent>();
    for (auto entity : view)
    {
        auto& transform = view.get<Lynx::TransformComponent>(entity);
        transform.Rotation.y += deltaTime * 1.0f;
        transform.Rotation.x += deltaTime * 0.5f;
    }
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
}