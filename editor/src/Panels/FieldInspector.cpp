#include "FieldInspector.h"

#include <Lynx/ImGui/LXUI.h>

namespace Lynx
{
    namespace
    {
        // Draws the control for one field. Returns true if the user changed the value.
        bool DrawFieldControl(const FieldInfo& field, FieldValue& value, Scene* scene)
        {
            switch (field.Kind)
            {
                case FieldKind::Bool:
                    return LXUI::DrawCheckBox(field.Label, std::get<bool>(value));

                case FieldKind::Integer:
                {
                    int number = static_cast<int>(std::get<int64_t>(value));
                    if (!LXUI::DrawDragInt(field.Label, number, field.DragSpeed, static_cast<int>(field.Min), static_cast<int>(field.Max)))
                        return false;

                    value = static_cast<int64_t>(number);
                    return true;
                }

                case FieldKind::Float:
                    return LXUI::DrawDragFloat(field.Label, std::get<float>(value), field.DragSpeed, field.Min, field.Max);

                case FieldKind::Vec2:
                    return LXUI::DrawVec2Control(field.Label, std::get<glm::vec2>(value), field.DragSpeed, field.Min, field.Max);

                case FieldKind::Vec3:
                    return LXUI::DrawVec3Control(field.Label, std::get<glm::vec3>(value), field.Min, field.Max);

                case FieldKind::Vec4:
                    return LXUI::DrawVec4Control(field.Label, std::get<glm::vec4>(value), field.Min, field.Max);

                case FieldKind::Color3:
                    return LXUI::DrawColor3Control(field.Label, std::get<glm::vec3>(value));

                case FieldKind::Color4:
                    return LXUI::DrawColorControl(field.Label, std::get<glm::vec4>(value));

                case FieldKind::Quat:
                {
                    glm::vec3 degrees = glm::degrees(glm::eulerAngles(std::get<glm::quat>(value)));
                    if (!LXUI::DrawVec3Control(field.Label, degrees, field.Min, field.Max))
                        return false;

                    value = glm::quat(glm::radians(degrees));
                    return true;
                }

                case FieldKind::String:
                    return LXUI::DrawTextInput(field.Label, std::get<std::string>(value));

                case FieldKind::UUID:
                    // Identifiers are assigned by the engine and not edited by hand.
                    return false;

                case FieldKind::Enum:
                {
                    int selected = static_cast<int>(std::get<int64_t>(value));
                    if (!LXUI::DrawComboControl(field.Label, selected, field.EnumNames))
                        return false;

                    value = static_cast<int64_t>(selected);
                    return true;
                }

                case FieldKind::Asset:
                    return LXUI::DrawAssetReference(field.Label, std::get<UUID>(value), { field.ReferencedAssetType });

                case FieldKind::UIElement:
                    return LXUI::DrawUIElementSelection(field.Label, std::get<UUID>(value), scene);
            }

            return false;
        }
    }

    void DrawFields(const std::vector<FieldInfo>& fields, void* component, Scene* scene)
    {
        for (const FieldInfo& field : fields)
        {
            if (!field.ShownInEditor)
                continue;

            FieldValue value = field.Get(component);
            if (DrawFieldControl(field, value, scene))
                field.Set(component, value);
        }
    }
}
