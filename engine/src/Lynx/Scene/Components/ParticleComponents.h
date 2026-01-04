#pragma once

#include <glm/glm.hpp>
#include "Lynx/UUID.h"

namespace Lynx
{
    struct ParticleProps
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Velocity = { 0.0f, 1.0f, 0.0f };
        glm::vec3 VelocityVariation = { 0.5f, 0.5f, 0.5f };

        glm::vec4 ColorBegin = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 ColorEnd = { 1.0f, 1.0f, 1.0f, 1.0f };

        float SizeBegin = 0.5f;
        float SizeEnd = 0.0f;
        float SizeVariation = 0.2f;

        float LifeTime = 1.0f;
    };

    struct Particle
    {
        glm::vec3 Position;
        glm::vec3 Velocity;

        glm::vec4 ColorBegin, ColorEnd;

        float Rotation = 0.0f;
        float SizeBegin, SizeEnd;

        float LifeTime = 1.0f;
        float LifeRemaining = 0.0f;

        bool Active = false;
    };

    struct ParticleEmitterComponent
    {
        ParticleProps Properties;

        AssetHandle Material = AssetHandle::Null();

        float EmissionRate = 5.0f;
        bool IsLooping = true;

        std::vector<Particle> ParticlePool;
        uint32_t PoolIndex = 0;
        float TimeSinceLastEmit = 0.0f;

        ParticleEmitterComponent(uint32_t maxParticles = 100)
        {
            ParticlePool.resize(maxParticles);
        }
    };
}
