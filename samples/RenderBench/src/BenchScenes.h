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
class Crowd
{
public:
    Crowd(Scene& scene, const ShapeLibrary& shapes, uint32_t count);

    void Update(Scene& scene, float deltaTime);

private:
    struct Mover
    {
        entt::entity Handle;
        glm::vec3 Home;
        float Radius;
        float Speed;
        float Phase;
    };

    std::vector<Mover> m_Movers;
    float m_Time = 0.0f;
};

// A city of tall blocks with small props in the streets between them. Seen from a street, the blocks
// hide nearly everything that lies inside the camera's view.
void CreateBlocks(Scene& scene, const ShapeLibrary& shapes, uint32_t blocksPerSide, uint32_t propsPerBlock);
