#include "CullPass.h"

#include "Lynx/Asset/Shader.h"
#include "Lynx/Renderer/GraphicsContext.h"
#include "Lynx/Renderer/Vulkan/ComputePipeline.h"
#include "Lynx/Renderer/Vulkan/SharedBindings.h"

namespace Lynx
{
    namespace
    {
        // Has to match the numthreads of Cull.slang.
        constexpr uint32_t InstancesPerGroup = 64;

        // Has to match CullConstants in Cull.slang.
        struct CullPushData
        {
            uint64_t Instances;
            uint64_t Transforms;
            uint64_t VisibleInstances;
            uint64_t Commands;
            uint64_t Batches;
            uint64_t Lods;
            uint64_t Frame;
            uint64_t Visibility;
            uint64_t Cursors;

            // Scattered instances are culled by the threads after those of the places.
            uint32_t PlaceCount;
            CullPhase Phase;
            CullStep Step;
            uint32_t PyramidTexture;
            uint32_t PyramidMipCount;
            uint32_t Padding;
            glm::ivec2 PyramidSize;
        };

        // Has to match RunConstants in CullRuns.slang.
        struct RunPushData
        {
            uint64_t Commands;
            uint64_t Cursors;
            uint64_t State;

            // The draw commands of the phase are these two ranges, one after the other.
            uint32_t First;
            uint32_t Count;
            uint32_t SecondFirst;
            uint32_t SecondCount;
        };

        // Between two steps: the next one reads what the one before wrote.
        void WaitForStep(vk::CommandBuffer commands)
        {
            const vk::MemoryBarrier written(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
            commands.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, written, {}, {});
        }
    }

    struct CullPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;

        // Cull.slang, which counts and writes, and CullRuns.slang, which hands out the runs in between.
        vk::Pipeline Pipeline;
        vk::Pipeline RunPipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
            {
                Device.destroyPipeline(Pipeline);
                Device.destroyPipeline(RunPipeline);
            }
        }
    };

    CullPass::CullPass(CullPhase phase)
        : m_Phase(phase)
    {
    }

    CullPass::~CullPass() = default;

    void CullPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Cull.slang");
        m_RunPipelineState.SetShader("Shaders/CullRuns.slang");
        UpdatePipelines();
    }

    void CullPass::UpdatePipelines()
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader, "Cull.slang", sizeof(CullPushData), false); });
        m_RunPipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader, "CullRuns.slang", sizeof(RunPushData), true); });
    }

    void CullPass::CreatePipeline(const Shader& shader, const char* name, size_t pushSize, bool handsOutRuns)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, name, pushSize))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        vk::Pipeline& target = handsOutRuns ? m_Vulkan->RunPipeline : m_Vulkan->Pipeline;
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = target]() { device.destroyPipeline(old); });
        target = pipeline;
    }

    bool CullPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        UpdatePipelines();

        // Without draw commands there is nothing to fill in. Without the pipelines the commands keep
        // drawing nothing, which is what the upload left them as.
        if (renderData.Culling.PlaceCount + renderData.Culling.ScatterChunkCount == 0 || !m_Vulkan->Pipeline || !m_Vulkan->RunPipeline)
            return false;

        resources.Computes = true;
        return true;
    }

    void CullPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        const MeshCulling& culling = renderData.Culling;

        CullPushData push{};
        push.Instances = renderData.MeshInstanceAddress;
        push.Transforms = renderData.MeshTransformAddress;
        push.VisibleInstances = renderData.VisibleInstanceAddress;
        push.Commands = culling.CommandAddress;
        push.Batches = culling.BatchAddress;
        push.Lods = culling.LodAddress;
        push.Frame = culling.FrameAddress;
        push.Visibility = culling.VisibilityAddress;
        push.Cursors = culling.CursorAddress;
        push.PlaceCount = culling.PlaceCount;
        push.Phase = m_Phase;
        push.PyramidTexture = renderData.DepthPyramid.TextureIndex;
        push.PyramidMipCount = renderData.DepthPyramid.MipCount;
        push.PyramidSize = { renderData.DepthPyramid.Width, renderData.DepthPyramid.Height };

        // The early phase fills the camera's first commands and the sun's, the late phase the camera's
        // others, which lie between the two.
        RunPushData runs{};
        runs.Commands = culling.CommandAddress;
        runs.Cursors = culling.CursorAddress;
        runs.State = culling.StateAddress;
        if (m_Phase == CullPhase::Early)
        {
            runs.First = 0;
            runs.Count = culling.CameraCommandsPerPhase;
            runs.SecondFirst = 2 * culling.CameraCommandsPerPhase;
            runs.SecondCount = culling.CommandCount - 2 * culling.CameraCommandsPerPhase;
        }
        else
        {
            runs.First = culling.CameraCommandsPerPhase;
            runs.Count = culling.CameraCommandsPerPhase;
        }

        // The places first, then one group for every piece of a cell of scattered instances.
        const uint32_t groups = GroupCountFor(culling.PlaceCount, InstancesPerGroup) + culling.ScatterChunkCount;
        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);

        // A scene with few instances: the CPU laid the runs out, and one step does it all.
        if (!culling.CountsFirst)
        {
            push.Step = CullStep::CountAndWrite;
            m_Vulkan->Shared->Push(commands, push);
            commands.dispatch(groups, 1, 1);
            return;
        }

        // Three steps, see CullStep, in one pass: as three passes they cost more than they do.
        push.Step = CullStep::Count;
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(groups, 1, 1);
        WaitForStep(commands);

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->RunPipeline);
        m_Vulkan->Shared->Push(commands, runs);
        commands.dispatch(1, 1, 1);
        WaitForStep(commands);

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);
        push.Step = CullStep::Write;
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(groups, 1, 1);
    }
}
