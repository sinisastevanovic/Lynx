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

    class Terrain;

    // A terrain of the scene and where its first corner lies.
    struct ExtractedTerrain
    {
        std::shared_ptr<Terrain> Asset;
        glm::vec3 Origin{ 0.0f };
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

        // How large the sun is in the sky, as an angle in degrees.
        float SunAngularSize = 0.53f;

        // The air above the scene. The Earth's unless the scene says otherwise.
        AtmosphereSettings Atmosphere;

        // How the brightness of the picture is chosen.
        ExposureSettings Exposure;

        // An editor view also gets the grid.
        bool IsEditorView = false;

        // How this view is shown: the lit picture, or one thing about it.
        ViewSettings Viewing;

        float DeltaTime = 0.0f;

        // The wind over the scene. Its time is the sum of the scene's frame times, so it stands
        // still with the scene, and a benchmark's frames show the same wind in every run.
        WindSettings Wind;
        float WindTime = 0.0f;
        float PreviousWindTime = 0.0f;

        std::vector<ExtractedParticles> Particles;
        std::vector<ExtractedTerrain> Terrains;
        std::vector<DebugLine> DebugLines;

        // In the order they are drawn, the last one on top.
        std::vector<std::shared_ptr<UICanvas>> Canvases;

        // Empties the lists and keeps their memory, so extracting every frame does not allocate every frame.
        void Clear()
        {
            Particles.clear();
            Terrains.clear();
            DebugLines.clear();
            Canvases.clear();
        }
    };
}
