#include "ShaderCompiler.h"

#include "Lynx/Log.h"
#include "Lynx/Renderer/ShaderCache.h"

#include <slang-com-ptr.h>
#include <slang.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Lynx
{
    namespace
    {
        std::string_view Text(slang::IBlob* blob)
        {
            if (!blob)
                return {};
            return { static_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize() };
        }

        bool ToShaderStage(SlangStage slangStage, ShaderStage& stage)
        {
            switch (slangStage)
            {
                case SLANG_STAGE_VERTEX:   stage = ShaderStage::Vertex; return true;
                case SLANG_STAGE_FRAGMENT: stage = ShaderStage::Fragment; return true;
                case SLANG_STAGE_COMPUTE:  stage = ShaderStage::Compute; return true;
                default:                   return false;
            }
        }

        // What a global shader parameter is, as far as binding it is concerned.
        // Returns false for a kind the renderer has no use for yet.
        bool ToBindingKind(slang::TypeLayoutReflection* type, ShaderBindingKind& kind)
        {
            switch (type->getKind())
            {
                case slang::TypeReflection::Kind::SamplerState:
                    kind = ShaderBindingKind::Sampler;
                    return true;

                case slang::TypeReflection::Kind::ConstantBuffer:
                    kind = ShaderBindingKind::UniformBuffer;
                    return true;

                case slang::TypeReflection::Kind::Resource:
                {
                    const SlangResourceShape shape = type->getResourceShape();
                    const bool isWritable = type->getResourceAccess() != SLANG_RESOURCE_ACCESS_READ;
                    const SlangResourceShape baseShape = static_cast<SlangResourceShape>(shape & SLANG_RESOURCE_BASE_SHAPE_MASK);

                    if (baseShape == SLANG_STRUCTURED_BUFFER || baseShape == SLANG_BYTE_ADDRESS_BUFFER)
                        kind = ShaderBindingKind::StorageBuffer;
                    else if (isWritable)
                        kind = ShaderBindingKind::StorageTexture;
                    else if (shape & SLANG_TEXTURE_COMBINED_FLAG)
                        kind = ShaderBindingKind::TextureWithSampler;
                    else
                        kind = ShaderBindingKind::SampledTexture;
                    return true;
                }

                default:
                    return false;
            }
        }

        // Reads from the linked program what it expects to be bound.
        bool Reflect(slang::ProgramLayout* layout, const std::filesystem::path& file, ShaderProgram& program)
        {
            for (unsigned index = 0; index < layout->getParameterCount(); ++index)
            {
                slang::VariableLayoutReflection* parameter = layout->getParameterByIndex(index);
                slang::TypeLayoutReflection* type = parameter->getTypeLayout();

                if (parameter->getCategory() == slang::ParameterCategory::PushConstantBuffer)
                {
                    program.PushConstantSize = static_cast<uint32_t>(type->getElementTypeLayout()->getSize());
                    continue;
                }

                ShaderBinding binding;
                binding.Name = parameter->getName();
                binding.Set = parameter->getBindingSpace();
                binding.Binding = parameter->getBindingIndex();
                if (type->isArray())
                    binding.Count = static_cast<uint32_t>(type->getTotalArrayElementCount());

                if (!ToBindingKind(type->unwrapArray(), binding.Kind))
                {
                    LX_CORE_ERROR("Shader {0}: parameter '{1}' is of a kind the renderer cannot bind.", file.string(), binding.Name);
                    return false;
                }

                program.Bindings.push_back(std::move(binding));
            }

            std::ranges::sort(program.Bindings, [](const ShaderBinding& a, const ShaderBinding& b)
            {
                return std::tie(a.Set, a.Binding) < std::tie(b.Set, b.Binding);
            });
            return true;
        }
    }

    namespace
    {
        // Marks the position a vertex shader writes as invariant: the GPU then computes it the same way
        // in every shader that uses the same expression. A pass that draws only where the depth equals
        // what an earlier pass wrote depends on that, and Slang has no way to ask for it.
        void MakePositionInvariant(std::vector<uint32_t>& spirv)
        {
            constexpr uint32_t HeaderWords = 5;
            constexpr uint32_t OpDecorate = 71;
            constexpr uint32_t OpMemberDecorate = 72;
            constexpr uint32_t DecorationBuiltIn = 11;
            constexpr uint32_t DecorationInvariant = 18;
            constexpr uint32_t BuiltInPosition = 0;

            for (size_t index = HeaderWords; index < spirv.size();)
            {
                const uint32_t wordCount = spirv[index] >> 16;
                const uint32_t opcode = spirv[index] & 0xFFFFu;
                if (wordCount == 0)
                    return;

                // The position is either a variable of its own or a member of a block of built-in outputs.
                if (opcode == OpDecorate && wordCount == 4 && spirv[index + 2] == DecorationBuiltIn && spirv[index + 3] == BuiltInPosition)
                {
                    const uint32_t decoration[] = { (3u << 16) | OpDecorate, spirv[index + 1], DecorationInvariant };
                    spirv.insert(spirv.begin() + static_cast<ptrdiff_t>(index + wordCount), std::begin(decoration), std::end(decoration));
                    return;
                }
                if (opcode == OpMemberDecorate && wordCount == 5 && spirv[index + 3] == DecorationBuiltIn && spirv[index + 4] == BuiltInPosition)
                {
                    const uint32_t decoration[] = { (4u << 16) | OpMemberDecorate, spirv[index + 1], spirv[index + 2], DecorationInvariant };
                    spirv.insert(spirv.begin() + static_cast<ptrdiff_t>(index + wordCount), std::begin(decoration), std::end(decoration));
                    return;
                }

                index += wordCount;
            }
        }
    }

    struct ShaderCompiler::SlangSession
    {
        Slang::ComPtr<slang::IGlobalSession> Global;
    };

    ShaderCompiler::ShaderCompiler(const ShaderCache& cache, std::filesystem::path moduleDirectory)
        : m_Cache(cache), m_ModuleDirectory(std::move(moduleDirectory))
    {
    }

    ShaderCompiler::~ShaderCompiler() = default;

    std::shared_ptr<const ShaderProgram> ShaderCompiler::Load(const std::filesystem::path& file)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (std::optional<ShaderProgram> cached = m_Cache.LoadProgram(file))
            return std::make_shared<const ShaderProgram>(std::move(*cached));

        LX_CORE_TRACE("Compiling shader {0}", file.string());

        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            LX_CORE_ERROR("Could not open shader file '{0}'", file.string());
            return nullptr;
        }
        std::stringstream source;
        source << stream.rdbuf();

        if (!m_Slang)
        {
            m_Slang = std::make_unique<SlangSession>();
            if (SLANG_FAILED(slang::createGlobalSession(m_Slang->Global.writeRef())))
            {
                LX_CORE_ERROR("Could not start the Slang shader compiler.");
                m_Slang.reset();
                return nullptr;
            }
        }

        // Changing any of these settings requires bumping the program version in ShaderCache.cpp.
        slang::TargetDesc target;
        target.format = SLANG_SPIRV;
        target.profile = m_Slang->Global->findProfile("spirv_1_5");

        const std::string moduleDirectory = m_ModuleDirectory.string();
        const char* searchPaths[] = { moduleDirectory.c_str() };

        slang::SessionDesc sessionDesc;
        sessionDesc.targets = &target;
        sessionDesc.targetCount = 1;
        sessionDesc.searchPaths = searchPaths;
        sessionDesc.searchPathCount = 1;
        // Matrices are stored the way GLM stores them.
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

        // Compiling happens once per change and is cached, so it may take its time to make the shader fast.
        slang::CompilerOptionEntry options[1];
        options[0].name = slang::CompilerOptionName::Optimization;
        options[0].value.kind = slang::CompilerOptionValueKind::Int;
        options[0].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_HIGH;
        sessionDesc.compilerOptionEntries = options;
        sessionDesc.compilerOptionEntryCount = 1;

        // A session per program: a session remembers the modules it has loaded, and a changed file has to be read again.
        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(m_Slang->Global->createSession(sessionDesc, session.writeRef())))
        {
            LX_CORE_ERROR("Could not create a Slang session for {0}", file.string());
            return nullptr;
        }

        const auto report = [&file](slang::IBlob* diagnostics)
        {
            if (const std::string_view text = Text(diagnostics); !text.empty())
                LX_CORE_ERROR("Shader {0}:\n{1}", file.string(), text);
        };

        Slang::ComPtr<slang::IBlob> diagnostics;
        const std::string moduleName = file.stem().string();
        const std::string filePath = file.string();
        slang::IModule* module = session->loadModuleFromSourceString(moduleName.c_str(), filePath.c_str(), source.str().c_str(), diagnostics.writeRef());
        report(diagnostics);
        if (!module)
            return nullptr;

        // The program is the module together with every entry point it marks with [shader(...)].
        std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints(module->getDefinedEntryPointCount());
        std::vector<slang::IComponentType*> components = { module };
        for (SlangInt32 index = 0; index < static_cast<SlangInt32>(entryPoints.size()); ++index)
        {
            module->getDefinedEntryPoint(index, entryPoints[index].writeRef());
            components.push_back(entryPoints[index]);
        }

        if (entryPoints.empty())
        {
            LX_CORE_ERROR("Shader {0} has no entry point. Mark one with [shader(\"vertex\")], \"fragment\" or \"compute\".", file.string());
            return nullptr;
        }

        Slang::ComPtr<slang::IComponentType> composite;
        diagnostics = nullptr;
        const SlangResult composed = session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), composite.writeRef(), diagnostics.writeRef());
        report(diagnostics);
        if (SLANG_FAILED(composed))
            return nullptr;

        Slang::ComPtr<slang::IComponentType> linked;
        diagnostics = nullptr;
        const SlangResult wasLinked = composite->link(linked.writeRef(), diagnostics.writeRef());
        report(diagnostics);
        if (SLANG_FAILED(wasLinked))
            return nullptr;

        slang::ProgramLayout* layout = linked->getLayout();

        ShaderProgram program;
        for (SlangInt index = 0; index < static_cast<SlangInt>(entryPoints.size()); ++index)
        {
            ShaderStageCode stage;
            if (!ToShaderStage(layout->getEntryPointByIndex(static_cast<SlangUInt>(index))->getStage(), stage.Stage))
            {
                LX_CORE_ERROR("Shader {0} has an entry point for a stage the renderer does not use.", file.string());
                return nullptr;
            }

            Slang::ComPtr<slang::IBlob> code;
            diagnostics = nullptr;
            const SlangResult compiled = linked->getEntryPointCode(index, 0, code.writeRef(), diagnostics.writeRef());
            report(diagnostics);
            if (SLANG_FAILED(compiled))
                return nullptr;

            const auto* words = static_cast<const uint32_t*>(code->getBufferPointer());
            stage.Spirv.assign(words, words + code->getBufferSize() / sizeof(uint32_t));
            if (stage.Stage == ShaderStage::Vertex)
                MakePositionInvariant(stage.Spirv);
            program.Stages.push_back(std::move(stage));
        }

        if (!Reflect(layout, file, program))
            return nullptr;

        // Everything the result depends on: the file itself and what it imports.
        std::vector<std::filesystem::path> dependencies = { file };
        for (SlangInt32 index = 0; index < module->getDependencyFileCount(); ++index)
        {
            const std::filesystem::path dependency = module->getDependencyFilePath(index);
            if (std::filesystem::exists(dependency) && std::ranges::find(dependencies, dependency) == dependencies.end())
                dependencies.push_back(dependency);
        }

        m_Cache.StoreProgram(file, dependencies, program);
        return std::make_shared<const ShaderProgram>(std::move(program));
    }
}
