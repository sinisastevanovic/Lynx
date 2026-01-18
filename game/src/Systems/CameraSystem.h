#pragma once
#include "Lynx.h"

#include "Components/GameComponents.h"

class CameraSystem
{
public:
    static void Update(std::shared_ptr<Scene> scene, float dt)
    {
        glm::vec3 playerPos = { 0, 0, 0 };
        auto playerView = scene->Reg().view<TransformComponent, PlayerComponent>();
        for (auto entity : playerView)
        {
            playerPos = playerView.get<TransformComponent>(entity).Translation;
            break;
        }

        auto cameraView = scene->Reg().view<TransformComponent, CameraComponent>();
        for (auto entity : cameraView)
        {
            auto [transform, camera] = cameraView.get<TransformComponent, CameraComponent>(entity);
            if (camera.Primary)
            {
                glm::vec3 targetPos = playerPos + glm::vec3(0.0f, 15.0f, 10.0f);

                float lerpFactor = 5.0f * dt;
                transform.Translation = glm::mix(transform.Translation, targetPos, glm::clamp(lerpFactor, 0.0f, 1.0f));
            }
        }
    }
};
