// Decides which mesh instances a frame draws: every instance of the scene is tested against the camera's
// view and the sun's, and those that are seen are added to the draw command of their batch.
//
// The CPU has laid the frame out: one draw command per batch and view that draws nothing yet, each with
// a run in the list of drawn instances that could hold the whole batch. Here an instance that is seen
// takes the next free place in the run and the command's count goes up by one. Many instances do that
// at the same time, so the count is raised atomically, which also hands every instance a place of its own.
// The order of the instances within a run therefore differs from frame to frame.

import Mesh;

// The planes around what a view sees, pointing inwards: the normal in xyz, the distance in w.
// Has to match CullView in GpuScene.cpp.
struct View
{
    float4 planes[6];
};

// The draw commands a batch's instances are counted into. noCommand for a view the batch is not drawn in.
// Has to match BatchCommands in GpuScene.cpp.
struct BatchCommands
{
    uint camera;
    uint sun;
};

// What a draw call reads from a buffer, as Vulkan defines it (VkDrawIndexedIndirectCommand).
struct DrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

static const uint noBatch = 0xFFFFFFFF;
static const uint noCommand = 0xFFFFFFFF;

struct CullConstants
{
    MeshInstance* instances;
    uint* visibleInstances;
    DrawCommand* commands;
    BatchCommands* batches;

    // The camera's view, then the sun's.
    View* views;

    // How many places the instance buffer has. Some of them may be empty.
    uint placeCount;
};
[vk::push_constant] ConstantBuffer<CullConstants> constants;

// False only if the sphere lies wholly behind one of the planes.
bool isSeen(uint view, float3 centre, float radius)
{
    for (uint plane = 0; plane < 6; ++plane)
    {
        float4 equation = constants.views[view].planes[plane];
        if (dot(equation.xyz, centre) + equation.w < -radius)
            return false;
    }
    return true;
}

void draw(uint command, uint place)
{
    uint countBefore;
    InterlockedAdd(constants.commands[command].instanceCount, 1, countBefore);
    constants.visibleInstances[constants.commands[command].firstInstance + countBefore] = place;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    // The last group reaches past the last place.
    uint place = thread.x;
    if (place >= constants.placeCount)
        return;

    uint batch = constants.instances[place].batch;
    if (batch == noBatch)
        return;

    BatchCommands batchCommands = constants.batches[batch];
    if (batchCommands.camera == noCommand && batchCommands.sun == noCommand)
        return;

    // The sphere around the instance in the world. Its radius grows by the largest of the three scales,
    // which keeps it around the mesh however unevenly the instance is scaled.
    MeshInstance instance = constants.instances[place];
    float3 axisX = meshDirection(instance, float3(1.0, 0.0, 0.0));
    float3 axisY = meshDirection(instance, float3(0.0, 1.0, 0.0));
    float3 axisZ = meshDirection(instance, float3(0.0, 0.0, 1.0));
    float scale = sqrt(max(dot(axisX, axisX), max(dot(axisY, axisY), dot(axisZ, axisZ))));
    float3 centre = mul(instance.model, float4(instance.boundingSphere.xyz, 1.0)).xyz;
    float radius = instance.boundingSphere.w * scale;

    if (batchCommands.camera != noCommand && isSeen(0, centre, radius))
        draw(batchCommands.camera, place);

    if (batchCommands.sun != noCommand && isSeen(1, centre, radius))
        draw(batchCommands.sun, place);
}
