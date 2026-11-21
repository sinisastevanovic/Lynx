#pragma once

#include "StaticMesh.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace Lynx
{
    // A mesh as it is after cooking: everything a StaticMesh needs from its file, ready to hand to the GPU.
    struct CookedMesh
    {
        AABB Bounds;
        std::vector<SubmeshSourceData> Submeshes;
    };

    // Keeps cooked meshes on disk, so a mesh is read from its model file and cooked only when that file
    // changes. Loading a mesh is then reading one file, with nothing to parse, decode or reorder.
    class MeshCache
    {
    public:
        explicit MeshCache(const std::filesystem::path& directory);

        // A cooked mesh, stored together with the files it was made from. Loading returns it only if
        // every one of those files still has the content it had then.
        std::optional<CookedMesh> Load(const std::filesystem::path& sourceFile) const;
        void Store(const std::filesystem::path& sourceFile, const std::vector<std::filesystem::path>& dependencies, const CookedMesh& mesh) const;

    private:
        std::filesystem::path EntryPath(const std::filesystem::path& sourceFile) const;

        std::filesystem::path m_Directory;
    };
}
