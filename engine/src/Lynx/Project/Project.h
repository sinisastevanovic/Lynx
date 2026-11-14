#pragma once

#include "Lynx/Core.h"

#include <expected>
#include <filesystem>
#include <string>

namespace Lynx
{
    // A game project, described by a .lxproj file in the project's root folder.
    struct LX_API Project
    {
        static constexpr const char* FileExtension = ".lxproj";

        std::string Name;
        std::filesystem::path RootDirectory;  // The folder that contains the .lxproj file.
        std::filesystem::path AssetDirectory; // Absolute.
        std::filesystem::path StartScene;     // Absolute. Empty if the project names none.
        std::string GameModule;               // Library with the game code, without extension. Empty if none.

        // Both return a message describing the problem on failure. They do not log,
        // because a project is loaded before the engine and its logger exist.
        static std::expected<Project, std::string> Load(const std::filesystem::path& projectFile);

        // Loads the project file named by the first command-line argument.
        // Without an argument, loads the only .lxproj file in the working directory.
        static std::expected<Project, std::string> LoadFromCommandLine(int argc, char** argv);
    };
}
