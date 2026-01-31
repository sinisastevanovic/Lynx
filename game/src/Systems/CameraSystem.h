#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

class CameraSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        // TODO: We should probably store the player entities somewhere, as they rarely change. 
        // Better than searching for the player everywhere each frame
        glm::vec3 targetPos = { 0, 0, 0 };
        bool targetFound = false;
        
        auto playerView = scene->Reg().view<TransformComponent, PlayerComponent>();
        for (auto entity : playerView)
        {
            targetPos = playerView.get<TransformComponent>(entity).Translation;
            targetFound = true;
            break;
        }
        
        if (!targetFound)
            return;
        
        // Update cameras
        auto cameraView = scene->Reg().view<TransformComponent, CameraComponent, SpringArmComponent>();
        for (auto entity : cameraView)
        {
            auto [transform, camera, spring] = cameraView.get<TransformComponent, CameraComponent, SpringArmComponent>(entity);
            if (!camera.Primary)
                continue;
            
            // Input handling
            glm::vec2  currentMousePos = Input::GetMousePosition(); // TODO: This is not correct, in editor we need viewport bound mouse pos!
            if (spring.IsFirstUpdate)
            {
                spring.LastMousePos = currentMousePos;
                spring.IsFirstUpdate = false;
            }
            
            //glm::vec2 delta = currentMousePos - spring.LastMousePos;
            glm::vec2 delta = Input::GetMouseDelta();
            spring.LastMousePos = currentMousePos;
            
            spring.Yaw -= delta.x * spring.SensitivityX;
            spring.Pitch += delta.y * spring.SensitivityY;
            spring.Pitch = glm::clamp(spring.Pitch, spring.MinPitch, spring.MaxPitch);
            
            glm::vec3 offset;
            offset.x = spring.TargetArmLength * cos(glm::radians(spring.Pitch)) * sin(glm::radians(spring.Yaw));
            offset.y = spring.TargetArmLength * sin(glm::radians(spring.Pitch));
            offset.z = spring.TargetArmLength * cos(glm::radians(spring.Pitch)) * cos(glm::radians(spring.Yaw));
            
            glm::vec3 lookAtTarget = targetPos + spring.TargetOffset;
            glm::vec3 finalPos = lookAtTarget + offset;
            
            //float lerpSpeed = spring.LerpSpeed * dt;
            //transform.Translation = glm::lerp(transform.Translation, finalPos, glm::clamp(lerpSpeed, 0.0f, 1.0f));
            transform.Translation = finalPos;
            transform.Rotation = glm::quatLookAt(glm::normalize(lookAtTarget - transform.Translation), glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }
};
