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
            uint64_t VisibleInstances;
            uint64_t Commands;
            uint64_t Batches;
            uint64_t Views;
            uint32_t PlaceCount;
            uint32_t Padding;
        };
    }

    struct CullPass::VulkanObjects
    {
        GraphicsContext* Context = nullptr;
        SharedBindings* Shared = nullptr;
        vk::Device Device;
        vk::Pipeline Pipeline;

        // The renderer waits for the GPU before it destroys its passes, so nothing here is still in use.
        ~VulkanObjects()
        {
            if (Device)
                Device.destroyPipeline(Pipeline);
        }
    };

    CullPass::CullPass() = default;
    CullPass::~CullPass() = default;

    void CullPass::Init(RenderContext& ctx)
    {
        m_Vulkan = std::make_unique<VulkanObjects>();
        m_Vulkan->Context = ctx.Graphics;
        m_Vulkan->Shared = ctx.Shared;
        m_Vulkan->Device = ctx.Graphics->Device();

        m_PipelineState.SetShader("Shaders/Cull.slang");
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });
    }

    void CullPass::CreatePipeline(const Shader& shader)
    {
        const std::shared_ptr<const ShaderProgram> program = shader.GetProgram();
        if (!program)
            return;

        // The pass and its shader are written together. If they stop agreeing, the pipeline that works is kept.
        if (!m_Vulkan->Shared->Supports(*program, "Cull.slang", sizeof(CullPushData)))
            return;

        const vk::Pipeline pipeline = CreateComputePipeline(m_Vulkan->Device, *program, m_Vulkan->Shared->PipelineLayout());
        if (!pipeline)
            return;

        // A reloaded shader replaces a pipeline that frames still on the GPU may be using.
        m_Vulkan->Context->Deletions().Retire([device = m_Vulkan->Device, old = m_Vulkan->Pipeline]() { device.destroyPipeline(old); });
        m_Vulkan->Pipeline = pipeline;
    }

    bool CullPass::Declare(const RenderContext&, const RenderData& renderData, PassResources& resources)
    {
        m_PipelineState.Update([this](std::shared_ptr<Shader> shader) { CreatePipeline(*shader); });

        // Without draw commands there is nothing to fill in. Without the pipeline the commands keep
        // drawing nothing, which is what the upload left them as.
        if (renderData.Culling.PlaceCount == 0 || !m_Vulkan->Pipeline)
            return false;

        resources.Computes = true;
        return true;
    }

    void CullPass::Execute(vk::CommandBuffer commands, RenderData& renderData)
    {
        CullPushData push{};
        push.Instances = renderData.MeshInstanceAddress;
        push.VisibleInstances = renderData.VisibleInstanceAddress;
        push.Commands = renderData.Culling.CommandAddress;
        push.Batches = renderData.Culling.BatchAddress;
        push.Views = renderData.Culling.ViewAddress;
        push.PlaceCount = renderData.Culling.PlaceCount;

        commands.bindPipeline(vk::PipelineBindPoint::eCompute, m_Vulkan->Pipeline);
        m_Vulkan->Shared->Push(commands, push);
        commands.dispatch(GroupCountFor(push.PlaceCount, InstancesPerGroup), 1, 1);
    }
}
