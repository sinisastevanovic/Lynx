#pragma once

#include "DebugRenderer.h"
#include "RenderPass.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace Lynx
{
    class Material;
    class UICanvas;

    // The particles of every emitter that uses one material.
    struct ExtractedParticles
    {
        std::shared_ptr<Material> Material;
        std::vector<ParticleInstanceData> Particles;
    };

    // What the renderer needs to draw one frame of a scene from one camera, copied out of the scene:
    // everything but the meshes. Those are too many to copy every frame. They stay on the GPU from frame
    // to frame, and only what changed about them is sent, see GpuScene.
    struct ExtractedScene
    {
        glm::mat4 View{ 1.0f };
        glm::mat4 Projection{ 1.0f };
        glm::vec3 CameraPosition{ 0.0f };

        // The direction the sun's light travels in.
        glm::vec3 SunDirection{ -0.5f, -0.7f, 1.0f };
        glm::vec3 SunColor{ 1.0f };
        float SunIntensity = 1.0f;

        // An editor view also gets the grid.
        bool IsEditorView = false;

        float DeltaTime = 0.0f;

        std::vector<ExtractedParticles> Particles;
        std::vector<DebugLine> DebugLines;

        // In the order they are drawn, the last one on top.
        std::vector<std::shared_ptr<UICanvas>> Canvases;

        // Empties the lists and keeps their memory, so extracting every frame does not allocate every frame.
        void Clear()
        {
            Particles.clear();
            DebugLines.clear();
            Canvases.clear();
        }
    };
}
