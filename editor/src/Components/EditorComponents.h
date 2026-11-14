#pragma once
#include <vector>
#include <string>

// Components of the editor have to be registered under a name that starts with "Editor".
// A game without the editor recognises them by that prefix and keeps their data without a warning.
struct EditorMetadataComponent
{
    std::vector<std::string> ComponentOrder;
};
