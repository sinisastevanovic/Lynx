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
    float SpawnRate = 2.0f;
    float Timer = 0.0f;
    int MaxEnemies = 50;
};