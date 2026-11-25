#pragma once

#include <Lynx.h>

#include <memory>
#include <vector>

// The scenes that are built in code when a benchmark starts. What they hold depends only on the numbers
// they are given, so every run of a benchmark draws the same scene.

// The shapes of Shapes.h as meshes, and a number of materials to give them: a few meshes that many
// entities share, each in a material of its choice.
class ShapeLibrary
{
public:
    static constexpr uint32_t BoxShape = 0;

    ShapeLibrary(AssetManager& assetManager, uint32_t materialCount);

    uint32_t ShapeCount() const { return static_cast<uint32_t>(m_Meshes.size()); }
    uint32_t MaterialCount() const { return static_cast<uint32_t>(m_Materials.size()); }
    const std::shared_ptr<StaticMesh>& Mesh(uint32_t shape) const { return m_Meshes[shape]; }
    const std::shared_ptr<Material>& MaterialAt(uint32_t material) const { return m_Materials[material]; }

private:
    std::vector<std::shared_ptr<StaticMesh>> m_Meshes;
    std::vector<std::shared_ptr<Material>> m_Materials;
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

// What anti-aliasing over time finds hard, put into a scene that has fine textures already: a box that
// keeps crossing in front of them, which leaves a trail if old frames are trusted too long, and a row of
// bars and a few wires that are thinner than a pixel from where the camera is, which flicker if they
// are trusted too little. Made for Sponza, in front of the end of its hall.
class MotionProps
{
public:
    MotionProps(Scene& scene, const ShapeLibrary& shapes);

    void Update(Scene& scene, float deltaTime);

private:
    entt::entity m_Mover = entt::null;
    float m_Time = 0.0f;
};

// A city of tall blocks with small props in the streets between them. Seen from a street, the blocks
// hide nearly everything that lies inside the camera's view.
void CreateBlocks(Scene& scene, const ShapeLibrary& shapes, uint32_t blocksPerSide, uint32_t propsPerBlock);

// A field of a few detailed models, each a hundred thousand triangles or more: scanned rocks and roots,
// plants with leaves, and models made of several parts. Where the other scenes are about the number of
// objects, this one is about the number of triangles and vertices.
void CreateDenseField(Scene& scene, AssetManager& assetManager, uint32_t count);

// A stand of trees, each an entity: nine trees of three kinds, from 0.4 to 7 million triangles each, as
// their makers modelled them, with every needle. It is what a forest costs before the renderer has levels
// of detail, shadows that reach far and trees that are not entities.
// The models are not in the repository: tools/fetch-polyhaven.ps1 fetches them.
void CreateTrees(Scene& scene, AssetManager& assetManager, uint32_t count);

// A square of hilly ground, made from noise: a terrain of the given number of heights a side, one
// unit apart, around the middle of the scene. The same hills in every run.
// throughFile saves the terrain and draws what is read back, to check the terrain's file.
void CreateTerrain(Scene& scene, AssetManager& assetManager, uint32_t resolution, bool throughFile);

// Checks that the terrain's ground is solid, and that it is the ground that is drawn. Gives the
// terrain a collider, drops balls onto it, and after a few frames shoots rays down at it. Writes
// to the log how far the rays' hits are off the terrain's own heights, and how the balls came to lie.
class TerrainPhysicsCheck
{
public:
    TerrainPhysicsCheck(Scene& scene, uint32_t balls);

    void Update(Scene& scene, float deltaTime);

private:
    void ShootRays(Scene& scene) const;
    void Report(Scene& scene) const;

    // Frames in which game time went on. It stands still while a benchmark warms up, and physics with it.
    uint32_t m_Frame = 0;

    // The least a ball's middle was above the ground under it, over all balls and frames.
    float m_LowestClearance = FLT_MAX;
};
