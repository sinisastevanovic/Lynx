#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace Lynx
{
    class GraphicsContext;
    struct ShaderProgram;

    // What every shader can reach without a pass binding anything for it: the frame's camera and sun,
    // and two tables with every texture and every sampler the renderer knows.
    //
    // It is one descriptor set and one pipeline layout for all passes. A shader picks a texture by its
    // index in the table, which it gets through its push constants or from the data of what it draws.
    // That is what lets one draw call cover objects with different textures.
    //
    // The shader side is Scene.slang and Textures.slang. The binding numbers below have to match them.
    class SharedBindings
    {
    public:
        static constexpr uint32_t SceneBinding = 0;
        static constexpr uint32_t TextureBinding = 1;
        static constexpr uint32_t SamplerBinding = 2;
        static constexpr uint32_t ComparisonSamplerBinding = 3;

        static constexpr uint32_t MaxTextures = 16384;
        static constexpr uint32_t MaxSamplers = 64;
        static constexpr uint32_t MaxComparisonSamplers = 8;

        // Every pipeline is made with room for this many bytes of push constants, the amount every GPU has.
        // Because all pipelines agree on it, the set stays bound when a pass switches to another pipeline.
        static constexpr uint32_t MaxPushConstantSize = 128;

        // The shader stages that can reach the set and the push constants: those that draw, and compute.
        // It is one list for everything, because Vulkan wants push constants set for exactly the stages
        // the layout names, whichever of them the pipeline at hand has.
        static constexpr vk::ShaderStageFlags ShaderStages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;

        SharedBindings(GraphicsContext& context, vk::Buffer sceneConstants);
        ~SharedBindings();

        SharedBindings(const SharedBindings&) = delete;
        SharedBindings& operator=(const SharedBindings&) = delete;

        // Puts a texture into the table and returns its index. The view has to stay alive until the
        // index is released, and the texture is expected in the layout for reading whenever a shader samples it.
        uint32_t AddTexture(vk::ImageView view);

        // Gives an index back. It is handed out again only after the frames that may still use it are done.
        void ReleaseTexture(uint32_t index);

        // The view that was added under an index.
        vk::ImageView TextureView(uint32_t index) const { return m_TextureViews[index]; }

        // Called with the index whenever a texture leaves the table, for code that keeps something per texture.
        void SetTextureReleaseListener(std::function<void(uint32_t)> listener) { m_ReleaseListener = std::move(listener); }

        // The index of a sampler, which is added to the table on first use. Samplers are never removed.
        uint32_t SamplerIndex(vk::Sampler sampler);

        // The same for a sampler that compares, which shaders reach through a table of its own.
        uint32_t ComparisonSamplerIndex(vk::Sampler sampler);

        // True if the set provides everything the program expects, and the program's push constants are
        // exactly as large as the data the pass sends. Logs what does not fit otherwise.
        bool Supports(const ShaderProgram& program, std::string_view shaderName, uint32_t pushConstantSize) const;

        vk::PipelineLayout PipelineLayout() const { return m_PipelineLayout; }

        // Binds the set for drawing or for compute. The two are bound apart: binding for one leaves the
        // other as it was.
        void Bind(vk::CommandBuffer commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics) const;

        // Sets the push constants of the draws that follow.
        template<typename Constants>
        void Push(vk::CommandBuffer commands, const Constants& constants) const
        {
            static_assert(sizeof(Constants) <= MaxPushConstantSize);
            commands.pushConstants(m_PipelineLayout, ShaderStages, 0, sizeof(Constants), &constants);
        }

    private:
        GraphicsContext& m_Context;
        vk::Device m_Device;

        vk::DescriptorSetLayout m_SetLayout;
        vk::PipelineLayout m_PipelineLayout;
        vk::DescriptorPool m_Pool;
        vk::DescriptorSet m_Set;

        // Indices that were released and can be handed out again. Shared with the deletion queue,
        // which may return an index after this object is gone.
        std::shared_ptr<std::vector<uint32_t>> m_FreeTextures;
        uint32_t m_TextureCount = 0;
        std::vector<vk::ImageView> m_TextureViews;
        std::function<void(uint32_t)> m_ReleaseListener;

        // The position of a sampler is its index.
        std::vector<vk::Sampler> m_Samplers;
        std::vector<vk::Sampler> m_ComparisonSamplers;

        uint32_t IndexOfSampler(std::vector<vk::Sampler>& table, vk::Sampler sampler, uint32_t binding, uint32_t capacity);
    };
}
