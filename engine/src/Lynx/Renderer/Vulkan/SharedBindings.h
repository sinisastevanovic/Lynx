#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
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

        static constexpr uint32_t MaxTextures = 16384;
        static constexpr uint32_t MaxSamplers = 64;

        // Every pipeline is made with room for this many bytes of push constants, the amount every GPU has.
        // Because all pipelines agree on it, the set stays bound when a pass switches to another pipeline.
        static constexpr uint32_t MaxPushConstantSize = 128;
        static constexpr vk::ShaderStageFlags PushConstantStages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        SharedBindings(GraphicsContext& context, vk::Buffer sceneConstants);
        ~SharedBindings();

        SharedBindings(const SharedBindings&) = delete;
        SharedBindings& operator=(const SharedBindings&) = delete;

        // Puts a texture into the table and returns its index. The view has to stay alive until the
        // index is released, and the texture is expected in the layout for reading whenever a shader samples it.
        uint32_t AddTexture(vk::ImageView view);

        // Gives an index back. It is handed out again only after the frames that may still use it are done.
        void ReleaseTexture(uint32_t index);

        // The index of a sampler, which is added to the table on first use. Samplers are never removed.
        uint32_t SamplerIndex(vk::Sampler sampler);

        // True if the set and the layout provide everything the program expects. Logs what is missing otherwise.
        bool Supports(const ShaderProgram& program, std::string_view shaderName) const;

        vk::PipelineLayout PipelineLayout() const { return m_PipelineLayout; }

        void Bind(vk::CommandBuffer commands) const;

        // Sets the push constants of the draws that follow.
        template<typename Constants>
        void Push(vk::CommandBuffer commands, const Constants& constants) const
        {
            static_assert(sizeof(Constants) <= MaxPushConstantSize);
            commands.pushConstants(m_PipelineLayout, PushConstantStages, 0, sizeof(Constants), &constants);
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

        // The position of a sampler is its index.
        std::vector<vk::Sampler> m_Samplers;
    };
}
