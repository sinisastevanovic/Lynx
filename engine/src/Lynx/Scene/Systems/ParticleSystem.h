#pragma once
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Components/ParticleComponents.h"

namespace Lynx
{
    class ParticleSystem
    {
    public:
        ParticleSystem() = default;

        void OnUpdate(float ts, Scene* scene, const glm::vec3& cameraPos);

    private:
        void EmitParticle(ParticleEmitterComponent& emitter, const glm::vec3& sourcePos);
    
    };
}
