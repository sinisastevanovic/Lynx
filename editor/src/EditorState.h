#pragma once

#include <Lynx/Window.h>

#include <filesystem>
#include <optional>

namespace Lynx
{
    // What the editor remembers between two sessions of the same project.
    struct EditorState
    {
        bool WindowMaximized = false;

        // Where the window sits when it is not maximized. Empty until the editor has been closed once.
        std::optional<WindowBounds> WindowBounds;

        // Returns the defaults if the file is missing or cannot be read.
        static EditorState Load(const std::filesystem::path& file);

        void Save(const std::filesystem::path& file) const;
    };
}
