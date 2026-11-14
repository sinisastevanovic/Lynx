#include "EditorState.h"

#include <Lynx/Log.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace Lynx
{
    namespace
    {
        std::optional<WindowBounds> ReadWindowBounds(const nlohmann::json& json)
        {
            const auto bounds = json.find("WindowBounds");
            if (bounds == json.end() || !bounds->is_object())
                return std::nullopt;

            for (const char* key : { "X", "Y", "Width", "Height" })
            {
                if (!bounds->contains(key) || !(*bounds)[key].is_number_integer())
                    return std::nullopt;
            }

            return WindowBounds{ (*bounds)["X"], (*bounds)["Y"], (*bounds)["Width"], (*bounds)["Height"] };
        }
    }

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

        state.WindowBounds = ReadWindowBounds(json);

        return state;
    }

    void EditorState::Save(const std::filesystem::path& file) const
    {
        nlohmann::json json;
        json["WindowMaximized"] = WindowMaximized;

        if (WindowBounds)
        {
            json["WindowBounds"] = {
                { "X", WindowBounds->X },
                { "Y", WindowBounds->Y },
                { "Width", WindowBounds->Width },
                { "Height", WindowBounds->Height }
            };
        }

        std::ofstream stream(file);
        stream << json.dump(4);
        if (!stream)
            LX_CORE_WARN("Cannot write editor state file: {0}", file.string());
    }
}
