#include "FieldSerialization.h"

#include "Lynx/Utils/JsonHelpers.h"

#include <set>

namespace Lynx
{
    namespace
    {
        nlohmann::json ToJson(const FieldInfo& field, const FieldValue& value)
        {
            switch (field.Kind)
            {
                case FieldKind::Bool:      return std::get<bool>(value);
                case FieldKind::Integer:
                case FieldKind::Enum:      return std::get<int64_t>(value);
                case FieldKind::Float:     return std::get<float>(value);
                case FieldKind::Vec2:      return std::get<glm::vec2>(value);
                case FieldKind::Vec3:
                case FieldKind::Color3:    return std::get<glm::vec3>(value);
                case FieldKind::Vec4:
                case FieldKind::Color4:    return std::get<glm::vec4>(value);
                case FieldKind::Quat:      return std::get<glm::quat>(value);
                case FieldKind::String:    return std::get<std::string>(value);
                case FieldKind::UUID:
                case FieldKind::Asset:
                case FieldKind::UIElement: return std::get<UUID>(value);
            }

            return nullptr;
        }

        FieldValue FromJson(const FieldInfo& field, const nlohmann::json& json)
        {
            switch (field.Kind)
            {
                case FieldKind::Bool:      return json.get<bool>();
                case FieldKind::Integer:
                case FieldKind::Enum:      return json.get<int64_t>();
                case FieldKind::Float:     return json.get<float>();
                case FieldKind::Vec2:      return json.get<glm::vec2>();
                case FieldKind::Vec3:
                case FieldKind::Color3:    return json.get<glm::vec3>();
                case FieldKind::Vec4:
                case FieldKind::Color4:    return json.get<glm::vec4>();
                case FieldKind::Quat:      return json.get<glm::quat>();
                case FieldKind::String:    return json.get<std::string>();
                case FieldKind::UUID:
                case FieldKind::Asset:
                case FieldKind::UIElement: return json.get<UUID>();
            }

            return false;
        }
    }

    void ValidateFields(const std::string& componentName, const std::vector<FieldInfo>& fields)
    {
        std::set<std::string> keys;
        for (const FieldInfo& field : fields)
        {
            if (!keys.insert(field.Key).second)
                LX_CORE_ERROR("Component '{0}' lists the field key '{1}' twice. Only one of the two would be saved.", componentName, field.Key);

            if (field.Kind == FieldKind::Enum && field.EnumNames.empty())
                LX_CORE_ERROR("Enum field '{1}' of component '{0}' has no names. Add .Names({{...}}) to it.", componentName, field.Key);
        }
    }

    void WriteFields(const std::vector<FieldInfo>& fields, const void* component, nlohmann::json& json)
    {
        for (const FieldInfo& field : fields)
            json[field.Key] = ToJson(field, field.Get(component));
    }

    void ReadFields(const std::vector<FieldInfo>& fields, void* component, const nlohmann::json& json)
    {
        for (const FieldInfo& field : fields)
        {
            const auto value = json.find(field.Key);
            if (value == json.end())
                continue;

            try
            {
                field.Set(component, FromJson(field, *value));
            }
            catch (const nlohmann::json::exception& exception)
            {
                LX_CORE_WARN("Ignoring the saved value of field '{0}': {1}", field.Key, exception.what());
            }
        }
    }
}
