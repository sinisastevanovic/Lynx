#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"
#include "Lynx/Scene/Components/PhysicsComponents.h"

class PlayerSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        auto& bodyInterface = scene->GetPhysicsSystem().GetBodyInterface();
        auto view = scene->Reg().view<TransformComponent, PlayerComponent, RigidBodyComponent, CharacterStatsComponent>();
        for (auto entity : view)
        {
            auto [transform, player, rb, stats] = view.get(entity);
            
            glm::vec3 camForward = { 0, 0, 1 };
            glm::vec3 camRight = { 1, 0, 0 };
            
            auto cameraView = scene->Reg().view<TransformComponent, CameraComponent>();
            for (auto camEntity : cameraView)
            {
                if (cameraView.get<CameraComponent>(camEntity).Primary)
                {
                    // TODO: Add GetForwardVector and GetRightVector and GetUpVector to TransformComponent
                    auto& ct = cameraView.get<TransformComponent>(camEntity);
                    camForward = ct.GetForward();
                    camForward.y = 0;
                    camForward = glm::normalize(camForward);
                    
                    camRight = ct.GetRight();
                    camRight.y = 0;
                    camRight = glm::normalize(camRight);
                    break;
                }
            }
            
            glm::vec3 moveDir = { 0.0f, 0.0f, 0.0f };
            moveDir += camForward * Input::GetAxis("MoveUpDown");
            moveDir += camRight * Input::GetAxis("MoveLeftRight");
            
            glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };

            if (glm::length(moveDir) > 0.001f)
            {
                moveDir = glm::normalize(moveDir);
                velocity = moveDir * stats.MoveSpeed;
                
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
        }
    }
};
