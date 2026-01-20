#pragma once


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
    
    };
}
