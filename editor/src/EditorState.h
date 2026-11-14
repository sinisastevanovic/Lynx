#pragma once

#include <filesystem>

namespace Lynx
{
    // What the editor remembers between two sessions of the same project.
    struct EditorState
    {
        bool WindowMaximized = false;

        // Returns the defaults if the file is missing or cannot be read.
        static EditorState Load(const std::filesystem::path& file);

        void Save(const std::filesystem::path& file) const;
    };
}
