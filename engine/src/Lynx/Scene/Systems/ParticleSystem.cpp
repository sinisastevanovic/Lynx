#include "ParticleSystem.h"

#include "Lynx/Renderer/ExtractedScene.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Components/ParticleComponents.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    float ParticleSystem::RandomRange(float min, float max)
    {
        // The generator's raw value is scaled by hand: the standard distributions may differ between compilers.
        const float unit = static_cast<float>(m_Random()) / static_cast<float>(std::mt19937::max());
        return min + unit * (max - min);
    }
    
    void ParticleSystem::Simulate(float deltaTime, Scene& scene)
    {
        auto view = scene.Reg().view<TransformComponent, ParticleEmitterComponent>();
        for (auto entity : view)
        {
            auto [transform, emitter] = view.get<TransformComponent, ParticleEmitterComponent>(entity);

            if (emitter.EmissionRate > 0.0f)
            {
                if (emitter.IsLooping)
                {
                    emitter.TimeSinceLastEmit += deltaTime;
                    const float emitInterval = 1.0f / emitter.EmissionRate;

                    while (emitter.TimeSinceLastEmit >= emitInterval)
                    {
                        emitter.TimeSinceLastEmit -= emitInterval;
                        EmitParticle(emitter, transform.GetWorldTranslation());
                    }
                }
                else if (!emitter.BurstDone)
                {
                    const int burstCount = static_cast<int>(emitter.EmissionRate);
                    for (int i = 0; i < burstCount; i++)
                        EmitParticle(emitter, transform.GetWorldTranslation());
                    emitter.BurstDone = true;
                }
            }

            for (Particle& particle : emitter.ParticlePool)
            {
                if (!particle.Active)
                    continue;

                if (particle.LifeRemaining <= 0.0f)
                {
                    particle.Active = false;
                    continue;
                }

                particle.LifeRemaining -= deltaTime;
                particle.Position += particle.Velocity * deltaTime;
                particle.Rotation += 0.01f * deltaTime;
            }
        }
    }

    void ParticleSystem::Extract(Scene& scene, const glm::vec3& cameraPosition, std::vector<ExtractedParticles>& particles) const
    {
        auto view = scene.Reg().view<ParticleEmitterComponent>();
        for (auto entity : view)
        {
            const ParticleEmitterComponent& emitter = view.get<ParticleEmitterComponent>(entity);
            if (!emitter.Material)
                continue;

            std::vector<ParticleInstanceData> instances;
            instances.reserve(emitter.ParticlePool.size());

            for (const Particle& particle : emitter.ParticlePool)
            {
                if (!particle.Active)
                    continue;

                const float life = particle.LifeRemaining / particle.LifeTime;

                ParticleInstanceData data;
                data.Position = particle.Position;
                data.Rotation = particle.Rotation;
                data.Color = glm::mix(particle.ColorEnd, particle.ColorBegin, life);
                data.Size = glm::mix(particle.SizeEnd, particle.SizeBegin, life);
                data.Life = life;
                instances.push_back(data);
            }

            if (instances.empty())
                continue;

            // TODO: Check if we actually need this?
            if (emitter.DepthSorting)
            {
                std::ranges::sort(instances, [cameraPosition](const ParticleInstanceData& a, const ParticleInstanceData& b)
                {
                    return glm::distance2(a.Position, cameraPosition) > glm::distance2(b.Position, cameraPosition);
                });
            }

            std::shared_ptr<Material> material = emitter.Material.Get();
            if (!material)
                continue;

            // Emitters that share a material are drawn with one call.
            const auto sameMaterial = std::ranges::find_if(particles, [&material](const ExtractedParticles& batch) { return batch.Material == material; });
            if (sameMaterial != particles.end())
                sameMaterial->Particles.insert(sameMaterial->Particles.end(), instances.begin(), instances.end());
            else
                particles.push_back({ std::move(material), std::move(instances) });
        }
    }

    void ParticleSystem::EmitParticle(ParticleEmitterComponent& emitter, const glm::vec3& sourcePos)
    {
        Particle& particle = emitter.ParticlePool[emitter.PoolIndex];
        const ParticleProps& props = emitter.Properties;
        particle.Active = true;

        particle.Position = sourcePos + props.Position;

        particle.Velocity = props.Velocity;
        particle.Velocity.x += props.VelocityVariation.x * (RandomRange(-0.5f, 0.5f));
        particle.Velocity.y += props.VelocityVariation.y * (RandomRange(-0.5f, 0.5f));
        particle.Velocity.z += props.VelocityVariation.z * (RandomRange(-0.5f, 0.5f));

        particle.ColorBegin = props.ColorBegin;
        particle.ColorEnd = props.ColorEnd;

        particle.SizeBegin = props.SizeBegin + props.SizeVariation * (RandomRange(-0.5f, 0.5f));
        particle.SizeEnd = props.SizeEnd;

        particle.LifeTime = props.LifeTime;
        particle.LifeRemaining = props.LifeTime;

        emitter.PoolIndex = (emitter.PoolIndex + 1) % emitter.ParticlePool.size();
    }
}
