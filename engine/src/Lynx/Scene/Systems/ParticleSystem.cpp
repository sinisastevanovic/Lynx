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
    
    void ParticleSystem::OnUpdate(float ts, Scene* scene, const glm::vec3& cameraPos)
    {
        auto view = scene->Reg().view<TransformComponent, ParticleEmitterComponent>();
        for (auto entity : view)
        {
            auto [trans, emitter] = view.get<TransformComponent, ParticleEmitterComponent>(entity);

            if (emitter.EmissionRate > 0.0f)
            {
                if (emitter.IsLooping)
                {
                    emitter.TimeSinceLastEmit += ts;
                    float emitInterval = 1.0f / emitter.EmissionRate;

                    while (emitter.TimeSinceLastEmit >= emitInterval)
                    {
                        emitter.TimeSinceLastEmit -= emitInterval;
                        EmitParticle(emitter, trans.Translation);
                    }
                }
                else
                {
                    if (!emitter.BurstDone)
                    {
                        int burstCount = (int)emitter.EmissionRate;
                        for (int i = 0; i < burstCount; i++)
                        {
                            EmitParticle(emitter, trans.Translation);
                        }
                        emitter.BurstDone = true;
                    }
                }
            }
            

            std::vector<ParticleInstanceData> activeParticles;
            activeParticles.reserve(emitter.ParticlePool.size());

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

                float life = particle.LifeRemaining / particle.LifeTime;

                ParticleInstanceData data;
                data.Position = particle.Position;
                data.Rotation = particle.Rotation;
                data.Color = glm::mix(particle.ColorEnd, particle.ColorBegin, life);
                data.Size = glm::mix(particle.SizeEnd, particle.SizeBegin, life);
                data.Life = life;
                activeParticles.push_back(data);
            }

            // TODO: Check if we actually need this?
            if (emitter.DepthSorting)
            {
                std::sort(activeParticles.begin(), activeParticles.end(),
                [cameraPos](const ParticleInstanceData& a, const ParticleInstanceData& b)
                {
                    float d1 = glm::distance2(a.Position, cameraPos);
                    float d2 = glm::distance2(b.Position, cameraPos);
                    return d1 > d2;
                });
            }

            if (!activeParticles.empty())
            {
                auto mat = Engine::Get().GetAssetManager().GetAsset<Material>(emitter.Material);
                if (mat)
                {
                    Engine::Get().GetRenderer().SubmitParticles(mat.get(), activeParticles);
                }
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

        emitter.PoolIndex = (emitter.PoolIndex + 1) % emitter.ParticlePool.size();
    }
}
