#pragma once

#include "Field.h"

#include <nlohmann/json_fwd.hpp>

namespace Lynx
{
    // Logs an error for mistakes in a field list that would otherwise show up as lost data:
    // two fields with the same key, or an enum field without names.
    LX_API void ValidateFields(const std::string& componentName, const std::vector<FieldInfo>& fields);

    // Writes every field of the component into json, under the field's key.
    LX_API void WriteFields(const std::vector<FieldInfo>& fields, const void* component, nlohmann::json& json);

    // Reads every field that json has a value for. A field without a value keeps what the component
    // already holds, which is its default for a newly created component. A value of the wrong type
    // is skipped with a warning.
    LX_API void ReadFields(const std::vector<FieldInfo>& fields, void* component, const nlohmann::json& json);
}
