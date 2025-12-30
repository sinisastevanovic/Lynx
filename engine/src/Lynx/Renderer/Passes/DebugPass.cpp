#include "DebugPass.h"

#include "Lynx/Renderer/DebugRenderer.h"
#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    void DebugPass::Init(RenderContext& ctx)
    {
        // 1. Create Layout
        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
            .setBindingOffsets({0, 0, 0, 0});
        m_BindingLayout = ctx.Device->createBindingLayout(layoutDesc);

        // 2. Create Binding Set (Scene UBO)
        auto bsDesc = nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, ctx.GlobalConstantBuffer));
        m_BindingSet = ctx.Device->createBindingSet(bsDesc, m_BindingLayout);

        // 3. Load Shader
        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>("engine/resources/Shaders/DebugLine.glsl");

        // 4. Create Pipeline
        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.bindingLayouts = {m_BindingLayout};
        pipeDesc.VS = shader->GetVertexShader();
        pipeDesc.PS = shader->GetPixelShader();

        // CRITICAL: Topology is LineList!
        pipeDesc.primType = nvrhi::PrimitiveType::LineList;

        // Input Layout
        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc().setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setBufferIndex(0).setOffset(0).setElementStride(
                sizeof(LineVertex)),
            nvrhi::VertexAttributeDesc().setName("COLOR").setFormat(nvrhi::Format::RGBA32_FLOAT).setBufferIndex(0).setOffset(sizeof(glm::vec3)).
                                         setElementStride(sizeof(LineVertex)),
        };
        pipeDesc.inputLayout = ctx.Device->createInputLayout(attributes, 2, shader->GetVertexShader());

        pipeDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false); // Usually off for debug lines so they don't occlude transparency

        m_Pipeline = ctx.Device->createGraphicsPipeline(pipeDesc, ctx.PresentationFramebufferInfo);

        // 5. Create Dynamic Vertex Buffer (Start small, resize if needed)
        nvrhi::BufferDesc vbDesc;
        vbDesc.byteSize = 10000 * sizeof(LineVertex); // ~10k lines capacity
        vbDesc.isVertexBuffer = true;
        vbDesc.debugName = "DebugLineVB";
        vbDesc.canHaveUAVs = false;
        vbDesc.setInitialState(nvrhi::ResourceStates::VertexBuffer);
        vbDesc.keepInitialState = true;
        m_VertexBuffer = ctx.Device->createBuffer(vbDesc);
    }

    void DebugPass::Execute(RenderContext& ctx, RenderData& renderData)
    {
        const auto& lines = DebugRenderer::GetLines();
        if (lines.empty()) return;

        ctx.CommandList->beginMarker("DebugPass");

        // 1. Convert Lines to Vertices
        std::vector<LineVertex> vertices;
        vertices.reserve(lines.size() * 2);
        for (const auto& line : lines)
        {
            vertices.push_back({line.Start, line.Color});
            vertices.push_back({line.End, line.Color});
        }

        // 2. Upload to Buffer
        // Check if we need to resize
        size_t dataSize = vertices.size() * sizeof(LineVertex);
        if (dataSize > m_VertexBuffer->getDesc().byteSize)
        {
            // TODO: add resizing if we need it!
        }

        // Write data
        ctx.CommandList->writeBuffer(m_VertexBuffer, vertices.data(), dataSize);

        // BARRIER! We just wrote to VB, now we read from it.
        ctx.CommandList->setBufferState(m_VertexBuffer, nvrhi::ResourceStates::VertexBuffer);

        // 3. Draw
        auto state = nvrhi::GraphicsState()
                     .setPipeline(m_Pipeline)
                     .setFramebuffer(renderData.TargetFramebuffer)
                     .addBindingSet(m_BindingSet)
                     .addVertexBuffer(nvrhi::VertexBufferBinding(m_VertexBuffer, 0, 0));

        // Viewport...
        const auto& fbInfo = renderData.TargetFramebuffer->getFramebufferInfo();
        state.viewport.addViewport(nvrhi::Viewport(fbInfo.width, fbInfo.height));
        state.viewport.addScissorRect(nvrhi::Rect(0, fbInfo.width, 0, fbInfo.height));

        ctx.CommandList->setGraphicsState(state);
        ctx.CommandList->draw(nvrhi::DrawArguments().setVertexCount(vertices.size()));

        ctx.CommandList->endMarker();

        renderData.DrawCalls++;
        renderData.IndexCount += vertices.size();

        // Clear lines for next frame
        DebugRenderer::Clear();
    }
}
