#pragma once

namespace Lynx
{
    class UIText;
    class UIImage;
    class UIButton;
}

struct PlayerComponent
{
    int PlayerIndex = 0;
    
    int JumpsRemaining = 0;
    bool IsGrounded = false;
    float JumpCooldown = 0.0f;
};

struct CharacterStatsComponent
{
    float Strength = 1.0f;
    float MoveSpeed = 5.0f;
    float JumpForce = 5.0f;
    int MaxJumpCount = 2; // TODO: 1 is default
    float MagnetRadius = 3.0f;
};

struct EnemyComponent
{
    float MoveSpeed = 2.5f;
    float XPValue = 10.0f;
};

struct WeaponComponent
{
    float FireRate = 0.5f;
    float Range = 5.0f;
    float Damage = 5.0f;
    float ProjectileSpeed = 20.0f;
    float CooldownTimer = 0.0f;
    AssetRef<Prefab> ProjectilePrefab;
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
    ElementRef<UIImage> HPBar;
    ElementRef<UIImage> XPBar;
    ElementRef<UIText> HPText;
    ElementRef<UIText> LevelText;
    ElementRef<UICanvas> LevelUpCanvas;
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
    float Strength = 10.0f;
};

struct DeadTag {};

struct SpringArmComponent // TODO: Maybe move to engine?
{
    float TargetArmLength = 15.0f;
    glm::vec3 TargetOffset = { 0.0f, 1.5f, 0.0f };
    
    float Pitch = 20.0f;
    float Yaw = 0.0f;
    
    float SensitivityX = 0.15f;
    float SensitivityY = 0.15f;
    
    float MinPitch = -10.0f;
    float MaxPitch = 85.0f;
    
    float LerpSpeed = 10.0f;
    
    glm::vec2 LastMousePos = { 0.0f, 0.0f };
    bool IsFirstUpdate = true;
};