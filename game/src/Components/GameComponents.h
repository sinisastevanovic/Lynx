#pragma once

struct PlayerComponent
{
    float MoveSpeed = 5.0f;
};

struct EnemyComponent
{
    float MoveSpeed = 2.5f;
    float Health = 10.0f; // TODO: Move to HealthComponent
};

struct EnemySpawnerComponent
{
    float SpawnRate = 1.0f;
    float Timer = 0.0f;
    int MaxEnemies = 50;
};

struct WeaponComponent
{
    float FireRate = 0.5f;
    float Range = 5.0f;
    float Damage = 5.0f;
    float ProjectileSpeed = 20.0f;
    float CooldownTimer = 0.0f;
    Lynx::AssetHandle ProjectilePrefab = Lynx::AssetHandle();
};

struct ProjectileComponent
{
    glm::vec3 Velocity = glm::vec3(0.0f);
    float Damage = 0.0f;
    float Lifetime = 3.0f;
    float Radius = 0.2f;
};