#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Lynx
{
    enum class ShaderStage : uint8_t
    {
        Vertex = 0,
        Fragment = 1,
        Compute = 2
    };

    // One bit per ShaderStage value.
    using ShaderStageMask = uint8_t;

    constexpr ShaderStageMask ToMask(ShaderStage stage)
    {
        return static_cast<ShaderStageMask>(1u << static_cast<uint8_t>(stage));
    }

    // What kind of thing a shader expects at a binding.
    enum class ShaderBindingKind : uint8_t
    {
        SampledTexture = 0,
        Sampler = 1,
        TextureWithSampler = 2,
        StorageTexture = 3,
        UniformBuffer = 4,
        StorageBuffer = 5
    };

    struct ShaderBinding
    {
        std::string Name;
        uint32_t Set = 0;
        uint32_t Binding = 0;
        ShaderBindingKind Kind = ShaderBindingKind::SampledTexture;

        // More than one for an array.
        uint32_t Count = 1;
    };

    struct ShaderStageCode
    {
        ShaderStage Stage = ShaderStage::Vertex;

        // The entry point is always called "main".
        std::vector<uint32_t> Spirv;
    };

    // A compiled shader program together with what it expects to be bound: the result of compiling
    // one .slang file. It is plain data. Pipelines and layouts are made from it by the renderer,
    // so no binding number is written by hand anywhere in C++.
    struct ShaderProgram
    {
        std::vector<ShaderStageCode> Stages;

        // Sorted by set, then binding.
        std::vector<ShaderBinding> Bindings;

        // Zero if the program has no push constants.
        uint32_t PushConstantSize = 0;

        ShaderStageMask StageMask() const
        {
            ShaderStageMask mask = 0;
            for (const ShaderStageCode& stage : Stages)
                mask |= ToMask(stage.Stage);
            return mask;
        }

        const ShaderStageCode* FindStage(ShaderStage stage) const
        {
            for (const ShaderStageCode& code : Stages)
            {
                if (code.Stage == stage)
                    return &code;
            }
            return nullptr;
        }
    };
}
