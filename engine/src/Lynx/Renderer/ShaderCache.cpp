#include "ShaderCache.h"

#include "Shader/ShaderProgram.h"

#include <fstream>

namespace Lynx
{
    namespace
    {
        // Bump this when the compile settings in ShaderUtils change, so entries built with the old settings are ignored.
        constexpr uint64_t s_CompileSettingsVersion = 1;

        constexpr uint32_t s_SpirvMagicNumber = 0x07230203;

        // Bump this when the format of a stored program or the compile settings in ShaderCompiler change.
        constexpr uint32_t s_ProgramVersion = 1;
        constexpr uint32_t s_ProgramMagicNumber = 0x5053584c; // "LXSP"

        // FNV-1a. std::hash is not guaranteed to give the same value in another build, and the keys are file names.
        uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= bytes[i];
                hash *= 0x100000001b3ull;
            }
            return hash;
        }
    }

    namespace
    {
        constexpr uint64_t s_HashSeed = 0xcbf29ce484222325ull;

        // Empty if the file cannot be read, which no stored hash equals.
        std::optional<uint64_t> HashFile(const std::filesystem::path& file)
        {
            std::ifstream stream(file, std::ios::binary | std::ios::ate);
            if (!stream)
                return std::nullopt;

            std::vector<char> content(static_cast<size_t>(stream.tellg()));
            stream.seekg(0);
            stream.read(content.data(), static_cast<std::streamsize>(content.size()));
            return HashBytes(s_HashSeed, content.data(), content.size());
        }

        // The stored form is a flat run of numbers and length-prefixed strings.
        class Writer
        {
        public:
            explicit Writer(std::ostream& stream) : m_Stream(stream) {}

            template<typename Number>
            void Write(Number value) { m_Stream.write(reinterpret_cast<const char*>(&value), sizeof(value)); }

            void Write(std::string_view text)
            {
                Write(static_cast<uint32_t>(text.size()));
                m_Stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            }

        private:
            std::ostream& m_Stream;
        };

        // Every read reports failure, so a truncated or damaged entry is treated as missing.
        class Reader
        {
        public:
            explicit Reader(std::istream& stream) : m_Stream(stream) {}

            template<typename Number>
            bool Read(Number& value) { return static_cast<bool>(m_Stream.read(reinterpret_cast<char*>(&value), sizeof(value))); }

            bool Read(std::string& text)
            {
                uint32_t size = 0;
                if (!Read(size) || size > MaxStringSize)
                    return false;
                text.resize(size);
                return static_cast<bool>(m_Stream.read(text.data(), size));
            }

        private:
            static constexpr uint32_t MaxStringSize = 4096;
            std::istream& m_Stream;
        };
    }

    std::optional<ShaderProgram> ShaderCache::LoadProgram(const std::filesystem::path& sourceFile) const
    {
        std::ifstream file(ProgramPath(sourceFile), std::ios::binary);
        if (!file)
            return std::nullopt;

        Reader reader(file);
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
            Writer writer(file);
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
        const uint64_t key = HashBytes(s_HashSeed, location.data(), location.size());

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

    uint64_t ShaderCache::MakeKey(std::string_view source, int stage)
    {
        uint64_t hash = 0xcbf29ce484222325ull;
        hash = HashBytes(hash, &s_CompileSettingsVersion, sizeof(s_CompileSettingsVersion));
        hash = HashBytes(hash, &stage, sizeof(stage));
        hash = HashBytes(hash, source.data(), source.size());
        return hash;
    }

    std::vector<uint32_t> ShaderCache::Load(uint64_t key) const
    {
        std::ifstream file(EntryPath(key), std::ios::binary | std::ios::ate);
        if (!file)
            return {};

        const std::streamsize byteCount = file.tellg();
        if (byteCount <= 0 || byteCount % sizeof(uint32_t) != 0)
            return {};

        std::vector<uint32_t> spirv(static_cast<size_t>(byteCount) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(spirv.data()), byteCount);

        // A truncated or foreign file is treated as a miss, and the shader is compiled again.
        if (!file || spirv.front() != s_SpirvMagicNumber)
            return {};

        return spirv;
    }

    void ShaderCache::Store(uint64_t key, const std::vector<uint32_t>& spirv) const
    {
        const std::filesystem::path entryPath = EntryPath(key);

        // Written under another name and renamed, so a reader never sees a half-written entry.
        std::filesystem::path temporaryPath = entryPath;
        temporaryPath += ".tmp";

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(spirv.data()), static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
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

    std::filesystem::path ShaderCache::EntryPath(uint64_t key) const
    {
        char fileName[32];
        std::snprintf(fileName, sizeof(fileName), "%016llx.spv", static_cast<unsigned long long>(key));
        return m_Directory / fileName;
    }
}
