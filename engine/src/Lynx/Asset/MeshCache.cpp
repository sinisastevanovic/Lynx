#include "MeshCache.h"

#include "Lynx/Core/BinaryFile.h"
#include "Lynx/Log.h"

#include <fstream>

namespace Lynx
{
    namespace
    {
        // Bump this when the stored form changes, or when cooking gives another result for the same file:
        // another vertex layout (PackedVertex), another draw order, other tangents.
        // 2: a material says whether it is two-sided.
        // 3: a submesh has levels of detail.
        constexpr uint32_t s_MeshVersion = 3;
        constexpr uint32_t s_MeshMagicNumber = 0x4d43584c; // "LXCM"

        // More than any mesh has. A damaged file that claims more is refused before memory is reserved for it.
        constexpr uint64_t s_LargestSubmeshCount = 1 << 20;
        constexpr uint64_t s_LargestVertexCount = uint64_t(1) << 31;
        constexpr uint64_t s_LargestIndexCount = uint64_t(1) << 32;

        void Write(BinaryWriter& writer, const SubmeshSourceData& submesh)
        {
            writer.Write(std::string_view(submesh.Name));
            writer.WriteArray(submesh.PackedVertices);
            writer.WriteArray(submesh.Indices);
            writer.WriteArray(submesh.Lods);
            writer.Write(submesh.BoundingSphere);

            const auto& material = submesh.MaterialData;
            writer.Write(material.AlbedoColor);
            writer.Write(material.Metallic);
            writer.Write(material.Roughness);
            writer.Write(material.EmissiveColor);
            writer.Write(material.EmissiveStrength);
            writer.Write(material.Mode);
            writer.Write(material.AlphaCutoff);
            writer.Write(material.TwoSided);
            writer.Write(std::string_view(material.AlbedoPath));
            writer.Write(std::string_view(material.NormalPath));
            writer.Write(std::string_view(material.MetallicRoughnessPath));
            writer.Write(std::string_view(material.EmissivePath));
        }

        bool Read(BinaryReader& reader, SubmeshSourceData& submesh)
        {
            auto& material = submesh.MaterialData;
            return reader.Read(submesh.Name)
                && reader.ReadArray(submesh.PackedVertices, s_LargestVertexCount)
                && reader.ReadArray(submesh.Indices, s_LargestIndexCount)
                && reader.ReadArray(submesh.Lods, MeshLod::MaxCount)
                && !submesh.Lods.empty()
                && reader.Read(submesh.BoundingSphere)
                && reader.Read(material.AlbedoColor)
                && reader.Read(material.Metallic)
                && reader.Read(material.Roughness)
                && reader.Read(material.EmissiveColor)
                && reader.Read(material.EmissiveStrength)
                && reader.Read(material.Mode)
                && reader.Read(material.AlphaCutoff)
                && reader.Read(material.TwoSided)
                && reader.Read(material.AlbedoPath)
                && reader.Read(material.NormalPath)
                && reader.Read(material.MetallicRoughnessPath)
                && reader.Read(material.EmissivePath);
        }
    }

    MeshCache::MeshCache(const std::filesystem::path& directory)
        : m_Directory(directory)
    {
        std::error_code error;
        std::filesystem::create_directories(m_Directory, error);
        if (error)
            LX_CORE_WARN("MeshCache: Cannot create {0}, meshes will be cooked on every start: {1}", m_Directory.string(), error.message());
    }

    std::optional<CookedMesh> MeshCache::Load(const std::filesystem::path& sourceFile) const
    {
        std::ifstream file(EntryPath(sourceFile), std::ios::binary);
        if (!file)
            return std::nullopt;

        BinaryReader reader(file);
        uint32_t magicNumber = 0;
        uint32_t version = 0;
        if (!reader.Read(magicNumber) || !reader.Read(version) || magicNumber != s_MeshMagicNumber || version != s_MeshVersion)
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

        CookedMesh mesh;
        uint64_t submeshCount = 0;
        if (!reader.Read(mesh.Bounds) || !reader.Read(submeshCount) || submeshCount > s_LargestSubmeshCount)
            return std::nullopt;

        mesh.Submeshes.resize(static_cast<size_t>(submeshCount));
        for (SubmeshSourceData& submesh : mesh.Submeshes)
        {
            if (!Read(reader, submesh))
                return std::nullopt;
        }

        return mesh;
    }

    void MeshCache::Store(const std::filesystem::path& sourceFile, const std::vector<std::filesystem::path>& dependencies, const CookedMesh& mesh) const
    {
        const std::filesystem::path entryPath = EntryPath(sourceFile);

        // Written under another name and renamed, so a reader never sees a half-written entry.
        std::filesystem::path temporaryPath = entryPath;
        temporaryPath += ".tmp";

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            BinaryWriter writer(file);
            writer.Write(s_MeshMagicNumber);
            writer.Write(s_MeshVersion);

            writer.Write(static_cast<uint32_t>(dependencies.size()));
            for (const std::filesystem::path& dependency : dependencies)
            {
                writer.Write(std::string_view(dependency.string()));

                // A file that cannot be read gets a hash that no later reading gives, so the entry is never used.
                writer.Write(HashFile(dependency).value_or(0));
            }

            writer.Write(mesh.Bounds);
            writer.Write(static_cast<uint64_t>(mesh.Submeshes.size()));
            for (const SubmeshSourceData& submesh : mesh.Submeshes)
                Write(writer, submesh);

            if (!file)
            {
                LX_CORE_WARN("MeshCache: Cannot write {0}", temporaryPath.string());
                return;
            }
        }

        std::error_code error;
        std::filesystem::rename(temporaryPath, entryPath, error);
        if (error)
            LX_CORE_WARN("MeshCache: Cannot store {0}: {1}", entryPath.string(), error.message());
    }

    std::filesystem::path MeshCache::EntryPath(const std::filesystem::path& sourceFile) const
    {
        // Named after the source file's location, so each mesh has one entry that is replaced when it changes.
        const std::string location = sourceFile.lexically_normal().generic_string();
        const uint64_t key = HashBytes(HashSeed, location.data(), location.size());

        char fileName[32];
        std::snprintf(fileName, sizeof(fileName), "%016llx.lxcm", static_cast<unsigned long long>(key));
        return m_Directory / fileName;
    }
}
