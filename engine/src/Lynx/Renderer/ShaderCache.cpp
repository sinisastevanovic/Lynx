#include "ShaderCache.h"

#include "Shader/ShaderProgram.h"
#include "Lynx/Core/BinaryFile.h"

#include <fstream>

namespace Lynx
{
    namespace
    {
        constexpr uint32_t s_SpirvMagicNumber = 0x07230203;

        // Bump this when the format of a stored program or the compile settings in ShaderCompiler change.
        constexpr uint32_t s_ProgramVersion = 3;
        constexpr uint32_t s_ProgramMagicNumber = 0x5053584c; // "LXSP"
    }

    std::optional<ShaderProgram> ShaderCache::LoadProgram(const std::filesystem::path& sourceFile) const
    {
        std::ifstream file(ProgramPath(sourceFile), std::ios::binary);
        if (!file)
            return std::nullopt;

        BinaryReader reader(file);
        uint32_t magicNumber = 0;
        uint32_t version = 0;
        if (!reader.Read(magicNumber) || !reader.Read(version) || magicNumber != s_ProgramMagicNumber || version != s_ProgramVersion)
            return std::nullopt;

        uint32_t dependencyCount = 0;
        if (!reader.Read(dependencyCount))
            return std::nullopt;

        for (uint32_t index = 0; index < dependencyCount; ++index)
        {
            std::string path;
            uint64_t storedHash = 0;
            if (!reader.Read(path) || !reader.Read(storedHash))
                return std::nullopt;

            if (HashFile(path) != storedHash)
                return std::nullopt;
        }

        ShaderProgram program;
        uint32_t bindingCount = 0;
        if (!reader.Read(program.PushConstantSize) || !reader.Read(bindingCount))
            return std::nullopt;

        program.Bindings.resize(bindingCount);
        for (ShaderBinding& binding : program.Bindings)
        {
            if (!reader.Read(binding.Name) || !reader.Read(binding.Set) || !reader.Read(binding.Binding) || !reader.Read(binding.Kind) || !reader.Read(binding.Count))
                return std::nullopt;
        }

        uint32_t stageCount = 0;
        if (!reader.Read(stageCount))
            return std::nullopt;

        program.Stages.resize(stageCount);
        for (ShaderStageCode& stage : program.Stages)
        {
            uint32_t wordCount = 0;
            if (!reader.Read(stage.Stage) || !reader.Read(wordCount))
                return std::nullopt;

            stage.Spirv.resize(wordCount);
            file.read(reinterpret_cast<char*>(stage.Spirv.data()), static_cast<std::streamsize>(wordCount * sizeof(uint32_t)));
            if (!file || stage.Spirv.empty() || stage.Spirv.front() != s_SpirvMagicNumber)
                return std::nullopt;
        }

        return program;
    }

    void ShaderCache::StoreProgram(const std::filesystem::path& sourceFile, const std::vector<std::filesystem::path>& dependencies, const ShaderProgram& program) const
    {
        const std::filesystem::path entryPath = ProgramPath(sourceFile);

        // Written under another name and renamed, so a reader never sees a half-written entry.
        std::filesystem::path temporaryPath = entryPath;
        temporaryPath += ".tmp";

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            BinaryWriter writer(file);
            writer.Write(s_ProgramMagicNumber);
            writer.Write(s_ProgramVersion);

            writer.Write(static_cast<uint32_t>(dependencies.size()));
            for (const std::filesystem::path& dependency : dependencies)
            {
                writer.Write(std::string_view(dependency.string()));
                writer.Write(HashFile(dependency).value_or(0));
            }

            writer.Write(program.PushConstantSize);
            writer.Write(static_cast<uint32_t>(program.Bindings.size()));
            for (const ShaderBinding& binding : program.Bindings)
            {
                writer.Write(std::string_view(binding.Name));
                writer.Write(binding.Set);
                writer.Write(binding.Binding);
                writer.Write(binding.Kind);
                writer.Write(binding.Count);
            }

            writer.Write(static_cast<uint32_t>(program.Stages.size()));
            for (const ShaderStageCode& stage : program.Stages)
            {
                writer.Write(stage.Stage);
                writer.Write(static_cast<uint32_t>(stage.Spirv.size()));
                file.write(reinterpret_cast<const char*>(stage.Spirv.data()), static_cast<std::streamsize>(stage.Spirv.size() * sizeof(uint32_t)));
            }

            if (!file)
            {
                LX_CORE_WARN("ShaderCache: Cannot write {0}", temporaryPath.string());
                return;
            }
        }

        std::error_code error;
        std::filesystem::rename(temporaryPath, entryPath, error);
        if (error)
            LX_CORE_WARN("ShaderCache: Cannot store {0}: {1}", entryPath.string(), error.message());
    }

    std::filesystem::path ShaderCache::ProgramPath(const std::filesystem::path& sourceFile) const
    {
        // Named after the source file's location, so each shader has one entry that is replaced when it changes.
        const std::string location = sourceFile.lexically_normal().generic_string();
        const uint64_t key = HashBytes(HashSeed, location.data(), location.size());

        char fileName[32];
        std::snprintf(fileName, sizeof(fileName), "%016llx.lxsp", static_cast<unsigned long long>(key));
        return m_Directory / fileName;
    }

    ShaderCache::ShaderCache(const std::filesystem::path& directory)
        : m_Directory(directory)
    {
        std::error_code error;
        std::filesystem::create_directories(m_Directory, error);
        if (error)
            LX_CORE_WARN("ShaderCache: Cannot create {0}, shaders will be compiled on every start: {1}", m_Directory.string(), error.message());
    }
}
