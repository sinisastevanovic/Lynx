#include "MyGameModule.h"

#include <Lynx/ComponentRegistry.h>
#include <Lynx/Scene/Scene.h>
#include <Lynx/Scene/Components/Components.h>
#include <Lynx/Scene/Entity.h>
#include <Lynx/Engine.h>

#include "Lynx/Input.h"
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
}

void MyGame::OnUpdate(float deltaTime)
{
    auto& engine = Lynx::Engine::Get();
    auto scene = engine.GetActiveScene();
    auto& bodyInterface = engine.GetPhysicsSystem().GetBodyInterface();
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

    glm::vec3 playerPos = { 0, 0, 0 };
    auto view = scene->Reg().view<Lynx::TransformComponent, PlayerComponent, Lynx::RigidBodyComponent>();
    for (auto entity : view)
    {
        auto [transform, player, rb] = view.get<Lynx::TransformComponent, PlayerComponent, Lynx::RigidBodyComponent>(entity);

        glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };

        if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::W)) velocity.z -= 1.0f;
        if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::S)) velocity.z += 1.0f;
        if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::A)) velocity.x -= 1.0f;
        if (Lynx::Input::IsKeyPressed(Lynx::KeyCode::D)) velocity.x += 1.0f;

        if (glm::length(velocity) > 0.0f)
        {
            velocity = glm::normalize(velocity) * player.MoveSpeed;

            float targetAngle = atan2(velocity.x, velocity.z);
            transform.Rotation = glm::angleAxis(targetAngle, glm::vec3(0, 1, 0));
        }

        if (rb.RuntimeBodyCreated)
        {
            JPH::BodyID bodyID = rb.BodyId;
            JPH::Vec3 currentVel = bodyInterface.GetLinearVelocity(bodyID);

            JPH::Vec3 newVel(velocity.x, currentVel.GetY(), velocity.z);
            JPH::Quat joltRot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);

            bodyInterface.ActivateBody(bodyID);
            bodyInterface.SetLinearVelocity(bodyID, newVel);
            bodyInterface.SetRotation(rb.BodyId, joltRot, JPH::EActivation::Activate);
        }

        playerPos = transform.Translation;
    }

    auto cameraView = scene->Reg().view<Lynx::TransformComponent, Lynx::CameraComponent>();
    for (auto entity : cameraView)
    {
        auto [transform, camera] = cameraView.get<Lynx::TransformComponent, Lynx::CameraComponent>(entity);
        if (camera.Primary)
        {
            glm::vec3 targetPos = playerPos + glm::vec3(0.0f, 15.0f, 10.0f);

            float lerpFactor = 5.0f * deltaTime;
            transform.Translation = glm::mix(transform.Translation, targetPos, glm::clamp(lerpFactor, 0.0f, 1.0f));
        }
    }
}

void MyGame::OnShutdown()
{
    LX_INFO("OnShutdown called.");
}