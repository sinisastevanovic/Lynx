#pragma once
#include "Lynx.h"
#include "Components/GameComponents.h"
#include "Lynx/Physics/PhysicsWorld.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class CharacterMovementSystem : public ISystem
{
public:
    std::string GetName() const override { return "CharacterMovementSystem"; }
    
    void OnUpdate(Scene& scene, float deltaTime) override
    {
        auto view = scene.View<CharacterControllerComponent, CharacterMovementComponent>();
        for (auto [entity, cc, movement] : view.each())
        {
            // --- Movement Input (Horizontal) ---
            glm::vec3 camForward = { 0, 0, 1 };
            glm::vec3 camRight = { 1, 0, 0 };
            
            auto cameraView = scene.View<TransformComponent, CameraComponent>();
            for (auto camEntity : cameraView)
            {
                if (cameraView.get<CameraComponent>(camEntity).Primary)
                {
                    auto& ct = cameraView.get<TransformComponent>(camEntity);
                    camForward = ct.GetForward();
                    camForward.y = 0; // Flatten
                    camForward = glm::normalize(camForward);
                    camRight = ct.GetRight();
                    camRight.y = 0;
                    camRight = glm::normalize(camRight);
                    break;
                }
            }

            glm::vec3 moveInput = { 0.0f, 0.0f, 0.0f };
            moveInput += camForward * Input::GetAxis("MoveUpDown");
            moveInput += camRight * Input::GetAxis("MoveLeftRight");
            
            movement.InputDirection = moveInput;
            movement.JumpInput = Input::GetButton("Jump");
        }
    }

    void OnFixedUpdate(Scene& scene, float fixedDeltaTime) override
    {
        auto& physics = scene.GetPhysicsWorldChecked();
        auto view = scene.View<TransformComponent, CharacterControllerComponent, CharacterMovementComponent>();
        for (auto [entity, transform, cc, movement] : view.each())
        {
            glm::vec3 gravity = physics.GetGravity();
            
            // --- Ground state tracking ---
            bool grounded = (cc.GroundState == GroundState::Grounded);
            if (grounded)
            {
                movement.TimeSinceGrounded = 0.0f;
                movement.JumpsRemaining = movement.MaxJumpCount;
            }
            else
            {
                movement.TimeSinceGrounded += fixedDeltaTime;
                // First jump used when leaving ground (unless within coyote time)
                if (movement.WasGrounded && movement.TimeSinceGrounded > movement.CoyoteTime)
                {
                    movement.JumpsRemaining = std::max(0, movement.JumpsRemaining - 1);
                }
            }
            movement.WasGrounded = grounded;
            
            // --- Jump input buffering ---
            bool jumpPressed = movement.JumpInput;
            if (jumpPressed)
            {
                movement.TimeSinceJumpPressed = 0.0f;
            }
            else
            {
                movement.TimeSinceJumpPressed += fixedDeltaTime;
            }
            movement.JumpHeld = jumpPressed;

            // --- Calculate horizontal movement ---
            float speed = movement.MoveSpeed;
            glm::vec3 horizontalVel = movement.InputDirection * speed;
            
            // Air control
            if (!grounded)
            {
                glm::vec3 currentHorizontal = glm::vec3(cc.Velocity.x, 0.0f, cc.Velocity.z);
                horizontalVel = glm::mix(currentHorizontal, horizontalVel, movement.AirControlFactor);
            }
            
            // --- Calculate vertical movement ---
            float verticalVelocity = cc.Velocity.y;
            
            // Check for jump
            bool canJump = movement.JumpsRemaining > 0;
            bool wantsJump = movement.TimeSinceJumpPressed < movement.JumpBufferTime;
            bool inCoyoteTime = movement.TimeSinceGrounded < movement.CoyoteTime;
            
            if (wantsJump && (canJump || inCoyoteTime))
            {
                verticalVelocity = movement.JumpVelocity;
                movement.JumpsRemaining--;
                movement.TimeSinceJumpPressed = 1000.0f;
                movement.TimeSinceGrounded = movement.CoyoteTime + 1.0f;
            }
            else if (grounded || cc.GroundState == GroundState::OnSteepGround)
            {
                verticalVelocity = -0.5f;
                horizontalVel += glm::vec3(cc.GroundVelocity.x, 0.0f, cc.GroundVelocity.z);
            }
            else
            {
                float gravityMultiplier = 1.0f;
                if (verticalVelocity > 0.0f && !movement.JumpHeld)
                {
                    verticalVelocity *= movement.JumpCutMultiplier;
                }
                
                if (verticalVelocity < 0.0f)
                {
                    gravityMultiplier = movement.FallGravityMultiplier;
                }
                
                verticalVelocity += gravity.y * gravityMultiplier * fixedDeltaTime;
            }
            
            cc.DesiredVelocity = glm::vec3(horizontalVel.x, verticalVelocity, horizontalVel.z);

            // --- Rotation ---
            if (glm::length2(movement.InputDirection) > 0.001f)
            {
                float targetAngle = atan2(movement.InputDirection.x, movement.InputDirection.z);
                glm::quat targetRotation = glm::angleAxis(targetAngle, glm::vec3(0, 1, 0));
                glm::quat newRotation = glm::slerp(transform.Rotation, targetRotation, 15.0f * fixedDeltaTime);
                physics.SetCharacterRotation(cc.CharacterId, newRotation);
            }
        }
    }
};
