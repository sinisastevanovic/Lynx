#pragma once

#include "ExtractedScene.h"
#include "Frustum.h"
#include "RenderPass.h"
#include "Vulkan/GpuBuffer.h"

#include <span>
#include <vector>

namespace Lynx
{
    class GraphicsContext;
    class MaterialTable;
    class UploadQueue;

    // Decides, every frame, which patches of the scene's terrains are drawn and how finely
    // (continuous distance-dependent level of detail, Strugar 2010).
    //
    // A terrain is a tree of square patches: the largest cover it whole, and each patch splits into four
    // of half the length. Every patch is drawn with the same small grid of triangles, so a small patch
    // is fine and a large one coarse. Close to the camera small patches are drawn, far away large ones:
    // a level of patches is used out to a distance that doubles with each level. Where only part of a
    // patch lies within the distance for the finer level, that part is drawn fine and the rest as
    // quarters of the coarse patch. The vertex shader slides the vertices of a patch towards those of
    // the next coarser level as it nears the end of its distance, so nothing jumps where levels meet
    // and no gaps open between them.
    //
    // The patches of a frame go to the GPU as one list for all views, and a view draws its part of
    // it with one draw call per kind: whole patches, and each of the four quarters.
    class TerrainRenderer
    {
    public:
        // The materials of a terrain's layers are entries of the material table, like those of meshes.
        TerrainRenderer(GraphicsContext& context, UploadQueue& uploads, MaterialTable& materials);

        // Picks the patches for the camera and for the sun's view of every cascade, and queues them for
        // upload. camera and sun are what each of them sees. The distance to the camera decides how fine
        // a patch is in every view: the ground's shadow is then cast by the ground as it is seen.
        TerrainViews Prepare(std::span<const ExtractedTerrain> terrains, const Frustum& camera, std::span<const Frustum, ShadowCascadeCount> sun,
            const glm::vec3& cameraPosition);

    private:
        struct Selection;
        void Select(Selection& selection, uint32_t level, uint32_t x, uint32_t z) const;

    private:
        GraphicsContext& m_Context;
        UploadQueue& m_Uploads;
        MaterialTable& m_Materials;

        // The triangles of one patch as indices into its grid of vertices, the four quarters one after
        // the other. A vertex has no data of its own: the shader works its place out from its index.
        GpuBuffer m_Indices;
        uint32_t m_IndicesPerQuarter = 0;

        // The patches and the terrains of the frame, rewritten every frame.
        GpuBuffer m_Patches;
        GpuBuffer m_Terrains;
    };
}
