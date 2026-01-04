#include "ParticleSystem.h"

#include <random>

#include "Lynx/Engine.h"
#include "Lynx/Scene/Components/Components.h"

namespace Lynx
{
    // TODO: Create Random class for engine
    static float RandomFloat()
    {
        static std::random_device rd;
        static std::mt19937 mt(rd());
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(mt);
    }

    static float RandomRange(float min, float max)
    {
        return min + RandomFloat() * (max - min);
    }
    
    void ParticleSystem::OnUpdate(float ts, Scene* scene)
    {
        auto view = scene->Reg().view<TransformComponent, ParticleEmitterComponent>();
        for (auto entity : view)
        {
            auto [trans, emitter] = view.get<TransformComponent, ParticleEmitterComponent>(entity);

            if (emitter.EmissionRate > 0.0f)
            {
                emitter.TimeSinceLastEmit += ts;
                float emitInterval = 1.0f / emitter.EmissionRate;

                while (emitter.TimeSinceLastEmit >= emitInterval)
                {
                    emitter.TimeSinceLastEmit -= emitInterval;
                    EmitParticle(emitter, trans.Translation);
                }
            }

            for (auto& particle : emitter.ParticlePool)
            {
                if (!particle.Active)
                    continue;

                if (particle.LifeRemaining <= 0.0f)
                {
                    particle.Active = false;
                    continue;
                }

                particle.LifeRemaining -= ts;
                particle.Position += particle.Velocity * ts;
                particle.Rotation += 0.01f * ts;
            }
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

        emitter.PoolIndex = --emitter.PoolIndex % emitter.ParticlePool.size();
    }
}
