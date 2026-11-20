#pragma once

#include "Lynx/Core.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Lynx
{
    // The arguments a program was started with, split into plain arguments and options.
    // An option starts with two dashes: "--name" is a flag and "--name=value" carries a value.
    class LX_API CommandLine
    {
    public:
        CommandLine(int argc, char** argv);

        // The arguments that are not options, in order. The program name is not one of them.
        const std::vector<std::string>& Arguments() const { return m_Arguments; }

        // True for "--name", with or without a value.
        bool Has(std::string_view name) const;

        // The value of the last "--name=value". Empty if the option is absent or has no value.
        std::optional<std::string> Value(std::string_view name) const;

        // The values of every "--name=value", in order.
        std::vector<std::string> Values(std::string_view name) const;

    private:
        struct Option
        {
            std::string Name;
            std::optional<std::string> Value;
        };

        std::vector<std::string> m_Arguments;
        std::vector<Option> m_Options;
    };
}
