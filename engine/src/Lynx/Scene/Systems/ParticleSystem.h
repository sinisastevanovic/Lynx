#pragma once

#include <glm/glm.hpp>

#include <random>
#include <vector>

namespace Lynx
{
    class Scene;
    struct ExtractedParticles;
    struct ParticleEmitterComponent;

    class ParticleSystem
    {
    public:
        ParticleSystem() = default;

        // Emits new particles and moves the living ones.
        void Simulate(float deltaTime, Scene& scene);

        // Appends what there is to draw, grouped by material. It changes nothing, so a frame can be drawn
        // any number of times, or not at all, without the particles moving.
        void Extract(Scene& scene, const glm::vec3& cameraPosition, std::vector<ExtractedParticles>& particles) const;

    private:
        void EmitParticle(ParticleEmitterComponent& emitter, const glm::vec3& sourcePos);
        float RandomRange(float min, float max);

    private:
        // Seeded with a constant, so a scene's particles are the same every time it is played.
        // Benchmarks and picture comparisons depend on that.
        std::mt19937 m_Random{ 0x4C594E58u };
    };
}
