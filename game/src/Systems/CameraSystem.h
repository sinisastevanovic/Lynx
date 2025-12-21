#pragma once
#include <memory>

#include "Components/GameComponents.h"
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Components/Components.h"

class CameraSystem
{
public:
    static void Update(std::shared_ptr<Lynx::Scene> scene, float dt)
    {
        glm::vec3 playerPos = { 0, 0, 0 };
        auto playerView = scene->Reg().view<Lynx::TransformComponent, PlayerComponent>();
        for (auto entity : playerView)
        {
            playerPos = playerView.get<Lynx::TransformComponent>(entity).Translation;
            break;
        }

        auto cameraView = scene->Reg().view<Lynx::TransformComponent, Lynx::CameraComponent>();
        for (auto entity : cameraView)
        {
            auto [transform, camera] = cameraView.get<Lynx::TransformComponent, Lynx::CameraComponent>(entity);
            if (camera.Primary)
            {
                glm::vec3 targetPos = playerPos + glm::vec3(0.0f, 15.0f, 10.0f);

                float lerpFactor = 5.0f * dt;
                transform.Translation = glm::mix(transform.Translation, targetPos, glm::clamp(lerpFactor, 0.0f, 1.0f));
            }
        }
    }
};
