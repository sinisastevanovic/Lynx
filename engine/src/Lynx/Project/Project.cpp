#include "Project.h"

#include "Lynx/Core/ContentPaths.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Lynx
{
    std::expected<Project, std::string> Project::Load(const std::filesystem::path& projectFile)
    {
        const std::filesystem::path filePath = ContentPaths::Normalize(projectFile);

        std::ifstream stream(filePath);
        if (!stream)
            return std::unexpected("Cannot open project file: " + filePath.string());

        const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
        if (!json.is_object())
            return std::unexpected("Project file is not a JSON object: " + filePath.string());

        try
        {
            Project project;
            project.RootDirectory = filePath.parent_path();
            project.Name = json.value("Name", filePath.stem().string());
            project.AssetDirectory = ContentPaths::Normalize(project.RootDirectory / json.value("AssetDirectory", "assets"));
            project.GameModule = json.value("GameModule", "");

            const std::string startScene = json.value("StartScene", "");
            if (!startScene.empty())
                project.StartScene = ContentPaths::Normalize(project.AssetDirectory / startScene);

            return project;
        }
        catch (const nlohmann::json::exception& exception)
        {
            return std::unexpected("Invalid project file " + filePath.string() + ": " + exception.what());
        }
    }

    std::expected<Project, std::string> Project::LoadFromCommandLine(int argc, char** argv)
    {
        if (argc > 1)
            return Load(argv[1]);

        const std::filesystem::path directory = std::filesystem::current_path();

        std::vector<std::filesystem::path> projectFiles;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == FileExtension)
                projectFiles.push_back(entry.path());
        }

        if (projectFiles.size() == 1)
            return Load(projectFiles.front());

        const char* reason = projectFiles.empty() ? "no" : "more than one";
        return std::unexpected(std::string("No project file was given, and there is ") + reason + " " + FileExtension + " file in " + directory.string());
    }
}
