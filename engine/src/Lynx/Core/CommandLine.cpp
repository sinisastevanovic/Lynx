#include "CommandLine.h"

namespace Lynx
{
    CommandLine::CommandLine(int argc, char** argv)
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (!argument.starts_with("--"))
            {
                m_Arguments.emplace_back(argument);
                continue;
            }

            const std::string_view option = argument.substr(2);
            const size_t separator = option.find('=');
            if (separator == std::string_view::npos)
                m_Options.push_back({ std::string(option), std::nullopt });
            else
                m_Options.push_back({ std::string(option.substr(0, separator)), std::string(option.substr(separator + 1)) });
        }
    }

    bool CommandLine::Has(std::string_view name) const
    {
        for (const Option& option : m_Options)
        {
            if (option.Name == name)
                return true;
        }
        return false;
    }

    std::optional<std::string> CommandLine::Value(std::string_view name) const
    {
        std::optional<std::string> value;
        for (const Option& option : m_Options)
        {
            if (option.Name == name && option.Value)
                value = option.Value;
        }
        return value;
    }

    std::vector<std::string> CommandLine::Values(std::string_view name) const
    {
        std::vector<std::string> values;
        for (const Option& option : m_Options)
        {
            if (option.Name == name && option.Value)
                values.push_back(*option.Value);
        }
        return values;
    }
}
