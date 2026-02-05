#pragma once

#include "Lynx.h"

#include "Components/GameComponents.h"
#include "Lynx/Renderer/DebugRenderer.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class PlayerSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        /*// --- Setup ---
        auto& physics = scene->GetPhysicsWorldChecked();
        static bool s_JumpButtonHeld = false; // TODO: Other solution in input class maybe? Probably just use events?
        
        auto view = scene->Reg().view<TransformComponent, PlayerComponent, CharacterControllerComponent, CharacterStatsComponent>();
        for (auto entity : view)
        {
            auto [transform, player, controller, stats] = view.get(entity);
            if (controller.CharacterId == INVALID_CHARACTER)
                continue;
            
            // --- Movement Input (Horizontal) ---
            glm::vec3 camForward = { 0, 0, 1 };
            glm::vec3 camRight = { 1, 0, 0 };
            
            auto cameraView = scene->Reg().view<TransformComponent, CameraComponent>();
            for (auto camEntity : cameraView)
            {
                if (cameraView.get<CameraComponent>(camEntity).Primary)
                {
                    auto& ct = cameraView.get<TransformComponent>(camEntity);
                    camForward = ct.GetForward();
                    camForward.y = 0; // TODO: Why??
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

            glm::vec3 targetHorizontalVel = { 0.0f, 0.0f, 0.0f };
            if (glm::length(moveInput) > 0.001f)
            {
                moveInput = glm::normalize(moveInput);
                targetHorizontalVel = moveInput * stats.MoveSpeed;
                
                float targetAngle = atan2(targetHorizontalVel.x, targetHorizontalVel.z);
                transform.Rotation = glm::angleAxis(targetAngle, glm::vec3(0.0f, 1.0f, 0.0f));
            }
            
            glm::vec3 currentPhysicsVel = controller.Velocity;
            float currentY = currentPhysicsVel.y;
            float gravity = controller.Gravity;
            float targetY = currentY + (gravity * dt);
            
            bool isGrounded = controller.GroundState == GroundState::Grounded;
            player.IsGrounded = isGrounded;
            
            if (isGrounded)
            {
                player.JumpsRemaining = stats.MaxJumpCount;
                if (targetY < 0.0f)
                    targetY = -2.0f;
            }
            
            bool jumpPressed = Input::GetButton("Jump");
            if (jumpPressed && !s_JumpButtonHeld)
            {
                if (player.JumpsRemaining > 0)
                {
                    targetY = stats.JumpForce;
                    player.JumpsRemaining--;
                }
            }
            s_JumpButtonHeld = jumpPressed;
            
            glm::vec3 finalVel(targetHorizontalVel.x, targetY, targetHorizontalVel.z);
            
            controller.DesiredVelocity = finalVel;
            //controller.Character->SetLinearVelocity(targetVel);*/
        //}
    }
};
