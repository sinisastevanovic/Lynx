// Gives every draw command of a phase of culling its run in the list of drawn instances.
//
// Cull.slang has counted, per command, the instances that are drawn with it. Here the commands get
// their runs, one behind the other, each as long as its count: a command's run starts where the counts
// of all commands before it end. Cull.slang then runs again and writes the instances into the runs.
// So the list holds what is drawn and nothing else, where a run for every instance a command might
// draw would be many times longer.
//
// One group of threads does it. The commands are dealt out in equal shares. Every thread adds up the
// counts of its share, all threads then learn the sums of the shares before theirs, and every thread
// hands out the runs of its share from there.

// What a draw call reads from a buffer, as Vulkan defines it (VkDrawIndexedIndirectCommand).
struct DrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

// How much of the list of drawn instances is given out, and how long the list is.
// Has to match CullState in GpuScene.cpp.
struct CullState
{
    uint used;
    uint capacity;
};

struct RunConstants
{
    DrawCommand* commands;

    // Per draw command, how many instances are written into its run so far: none yet.
    uint* cursors;
    CullState* state;

    // The draw commands of the phase are these two ranges, one after the other.
    uint first;
    uint count;
    uint secondFirst;
    uint secondCount;
};
[vk::push_constant] ConstantBuffer<RunConstants> constants;

// Has to match the numthreads below.
static const uint threadCount = 256;

static groupshared uint countOfShare[threadCount];

// The command that stands at a place among the commands of the phase.
uint commandAt(uint index)
{
    return index < constants.count ? constants.first + index : constants.secondFirst + (index - constants.count);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void computeMain(uint3 threadInGroup : SV_GroupThreadID)
{
    uint thread = threadInGroup.x;
    uint commandCount = constants.count + constants.secondCount;
    uint perShare = (commandCount + threadCount - 1) / threadCount;
    uint first = min(thread * perShare, commandCount);
    uint end = min(first + perShare, commandCount);

    // Read before the threads meet: the last thread writes it afterwards.
    uint usedBefore = constants.state.used;
    uint capacity = constants.state.capacity;

    uint inShare = 0;
    for (uint index = first; index < end; ++index)
        inShare += constants.commands[commandAt(index)].instanceCount;
    countOfShare[thread] = inShare;

    GroupMemoryBarrierWithGroupSync();

    uint start = usedBefore;
    for (uint share = 0; share < thread; ++share)
        start += countOfShare[share];

    for (uint index = first; index < end; ++index)
    {
        uint command = commandAt(index);

        // The list is long enough for everything that can be drawn. A run is cut short all the same
        // rather than let reach past its end, if a fault ever made the counts too large.
        uint count = min(constants.commands[command].instanceCount, capacity - min(start, capacity));
        constants.commands[command].instanceCount = count;
        constants.commands[command].firstInstance = start;
        constants.cursors[command] = 0;
        start += count;
    }

    // The last thread ends where the runs of the phase end.
    if (thread == threadCount - 1)
        constants.state.used = start;
}
