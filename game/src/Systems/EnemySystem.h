#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"
#include "Lynx/Asset/AssetManager.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class EnemySystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        auto& engine = Engine::Get();
        auto& assetManager = engine.GetAssetManager();

        // --- Spawning Logic ---
        auto spawnerView = scene->Reg().view<EnemySpawnerComponent>();
        for (auto spawnerEntity : spawnerView)
        {
            auto& spawner = spawnerView.get<EnemySpawnerComponent>(spawnerEntity);
            spawner.Timer -= dt;

            if (spawner.Timer <= 0.0f)
            {
                SpawnEnemy(scene, assetManager);
                spawner.Timer = spawner.SpawnRate;
            }
        }

        // --- AI / Movement Logic
        glm::vec3 playerPos = { 0, 0, 0 };
        auto playerView = scene->Reg().view<TransformComponent, PlayerComponent>();
        for (auto pEntity : playerView)
        {
            playerPos = playerView.get<TransformComponent>(pEntity).Translation;
            break;
        }

        auto& bodyInterface = scene->GetPhysicsSystem().GetBodyInterface();
        auto enemyView = scene->Reg().view<TransformComponent, EnemyComponent, RigidBodyComponent>();
        for (auto eEntity : enemyView)
        {
            auto [transform, enemy, rb] = enemyView.get<TransformComponent, EnemyComponent, RigidBodyComponent>(eEntity);

            glm::vec3 direction = playerPos - transform.Translation;
            direction.y = 0;

            if (glm::length(direction) > 0.1f)
            {
                direction = glm::normalize(direction);

                float targetAngle = atan2(direction.x, direction.z);
                transform.Rotation = glm::angleAxis(targetAngle, glm::vec3(0, 1, 0));

                if (rb.RuntimeBodyCreated)
                {
                    JPH::Vec3 currentVel = bodyInterface.GetLinearVelocity(rb.BodyId);
                    JPH::Vec3 newVel(direction.x * enemy.MoveSpeed, currentVel.GetY(), direction.z * enemy.MoveSpeed);

                    bodyInterface.ActivateBody(rb.BodyId);
                    bodyInterface.SetLinearVelocity(rb.BodyId, newVel);

                    JPH::Quat joltRot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
                    bodyInterface.SetRotation(rb.BodyId, joltRot, JPH::EActivation::Activate);
                }
            }
        }
    }

private:
    // TODO: AssetManager should not be in here. 
    static void SpawnEnemy(std::shared_ptr<Scene> scene, AssetManager& assetManager)
    {
        // Spawn in random circle around the origin (for now)
        // TODO: Spawn outside of camera view
        float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
        float radius = 20.0f;
        glm::vec3 spawnPos = { cos(angle) * radius, 1.0f, sin(angle) * radius };

        auto enemy = scene->CreateEntity("Enemy");
        auto& transform = enemy.GetComponent<TransformComponent>();
        transform.Translation = spawnPos;
        transform.Scale = { 2.0f, 2.0f, 2.0f };

        enemy.AddComponent<EnemyComponent>();

        auto& mesh = enemy.AddComponent<MeshComponent>();
        mesh.Mesh = assetManager.GetAsset("assets/Models/Bottle/WaterBottle.gltf")->GetHandle();

        auto& rb = enemy.AddComponent<RigidBodyComponent>();
        rb.Type = RigidBodyType::Dynamic;
        rb.LockAllRotation();

        auto& collider = enemy.AddComponent<CapsuleColliderComponent>();
        collider.Radius = 0.4f;
        collider.HalfHeight = 0.9f;
    }
};
