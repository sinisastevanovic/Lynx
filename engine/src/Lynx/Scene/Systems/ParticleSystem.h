#pragma once

#include <glm/glm.hpp>

#include <random>

namespace Lynx
{
    class Scene;
    class ParticleEmitterComponent;

    class ParticleSystem
    {
    public:
        ParticleSystem() = default;

        void OnUpdate(float ts, Scene* scene, const glm::vec3& cameraPos);

    private:
        void EmitParticle(ParticleEmitterComponent& emitter, const glm::vec3& sourcePos);
        float RandomRange(float min, float max);

    private:
        // Seeded with a constant, so a scene's particles are the same every time it is played.
        // Benchmarks and picture comparisons depend on that.
        std::mt19937 m_Random{ 0x4C594E58u };
    };
}
