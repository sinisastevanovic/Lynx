#pragma once

namespace Lynx
{
    class UIText;
    class UIImage;
}

struct PlayerComponent
{
    float MoveSpeed = 5.0f;
};

struct EnemyComponent
{
    float MoveSpeed = 2.5f;
    float XPValue = 10.0f;
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
    AssetHandle ProjectilePrefab = AssetHandle();
};

struct ProjectileComponent
{
    glm::vec3 Velocity = glm::vec3(0.0f);
    float Damage = 0.0f;
    float Lifetime = 3.0f;
    float Radius = 0.2f;
    
    entt::entity Owner = entt::null;
};

struct PlayerHUDComponent
{
    UUID HPBarID = UUID::Null();
    UUID HPTextID = UUID::Null();
    UUID XPBarID = UUID::Null();
    UUID LevelTextID = UUID::Null();
    
    mutable std::weak_ptr<UIImage> CachedHPBar;
    mutable std::weak_ptr<UIImage> CachedXPBar;
    mutable std::weak_ptr<UIText> CachedHPText;
    mutable std::weak_ptr<UIText> CachedLevelText;
};

struct HealthComponent
{
    float CurrentHealth = 100.0f;
    float MaxHealth = 100.0f;
};

struct ExperienceComponent
{
    float CurrentXP = 0.0f;
    float TargetXP = 100.0f;
    int Level = 1;
    mutable int LastUILevel = 0;
};

enum class PickupType { XP, Health };

struct PickupComponent
{
    PickupType Type = PickupType::XP;
    float Value = 10.0f;
    bool Magnetic = false;
    
    bool IsMagnetized = false;
    entt::entity MagentizedBy = entt::null;
};

struct MagnetComponent
{
    float Radius = 3.0f;
    float Strength = 10.0f;
};

struct DeadTag {};