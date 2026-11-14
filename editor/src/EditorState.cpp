#include "EditorState.h"

#include <Lynx/Log.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace Lynx
{
    EditorState EditorState::Load(const std::filesystem::path& file)
    {
        EditorState state;

        std::ifstream stream(file);
        if (!stream)
            return state;

        const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
        if (!json.is_object())
        {
            LX_CORE_WARN("Ignoring unreadable editor state file: {0}", file.string());
            return state;
        }

        const auto windowMaximized = json.find("WindowMaximized");
        if (windowMaximized != json.end() && windowMaximized->is_boolean())
            state.WindowMaximized = windowMaximized->get<bool>();

        return state;
    }

    void EditorState::Save(const std::filesystem::path& file) const
    {
        nlohmann::json json;
        json["WindowMaximized"] = WindowMaximized;

        std::ofstream stream(file);
        stream << json.dump(4);
        if (!stream)
            LX_CORE_WARN("Cannot write editor state file: {0}", file.string());
    }
}
