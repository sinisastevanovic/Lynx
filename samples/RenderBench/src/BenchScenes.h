#pragma once

#include <Lynx.h>

#include <memory>
#include <vector>

// The scenes that are built in code when a benchmark starts. What they hold depends only on the numbers
// they are given, so every run of a benchmark draws the same scene.

// Every shape of Shapes.h once per material: a few meshes with many materials. A mesh has its material
// built in today, so each combination is a mesh of its own.
class ShapeLibrary
{
public:
    static constexpr uint32_t BoxShape = 0;

    ShapeLibrary(AssetManager& assetManager, uint32_t materialCount);

    uint32_t ShapeCount() const { return m_ShapeCount; }
    uint32_t MaterialCount() const { return m_MaterialCount; }
    const std::shared_ptr<StaticMesh>& Mesh(uint32_t shape, uint32_t material) const { return m_Meshes[shape * m_MaterialCount + material]; }

private:
    uint32_t m_ShapeCount = 0;
    uint32_t m_MaterialCount = 0;
    std::vector<std::shared_ptr<StaticMesh>> m_Meshes;
};

// A square field of copies of three meshes.
void CreateInstanceGrid(Scene& scene, AssetManager& assetManager, uint32_t count);

// A field of shapes in many materials. One in five moves in a circle and bobs up and down.
// On request, some of those that stand still are replaced in every frame: removed, with a new one put
// in their place. That is what a game does when it spawns and destroys things while it runs.
class Crowd
{
public:
    Crowd(Scene& scene, const ShapeLibrary& shapes, uint32_t count, uint32_t replacedPerFrame);

    void Update(Scene& scene, float deltaTime);

private:
    // Puts the prop with the given number at a place in the field. The number decides what it looks like.
    Entity Place(Scene& scene, uint32_t cell, uint32_t propNumber, glm::vec3& position) const;

    struct Mover
    {
        entt::entity Handle;
        glm::vec3 Home;
        float Radius;
        float Speed;
        float Phase;
    };

    struct Standing
    {
        entt::entity Handle;
        uint32_t Cell;
    };

    ShapeLibrary m_Shapes;
    uint32_t m_Side = 0;
    std::vector<Mover> m_Movers;
    std::vector<Standing> m_Standing;
    uint32_t m_ReplacedPerFrame = 0;
    uint32_t m_NextPropNumber = 0;
    float m_Time = 0.0f;
};

// A city of tall blocks with small props in the streets between them. Seen from a street, the blocks
// hide nearly everything that lies inside the camera's view.
void CreateBlocks(Scene& scene, const ShapeLibrary& shapes, uint32_t blocksPerSide, uint32_t propsPerBlock);

// A field of a few detailed models, each a hundred thousand triangles or more: scanned rocks and roots,
// plants with leaves, and models made of several parts. Where the other scenes are about the number of
// objects, this one is about the number of triangles and vertices.
void CreateDenseField(Scene& scene, AssetManager& assetManager, uint32_t count);
