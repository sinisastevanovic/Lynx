#pragma once

#include <Lynx/Reflection/Field.h>

namespace Lynx
{
    class Scene;

    // Draws one inspector row per field of a component, and writes edits back into the component.
    // Call it between LXUI::BeginPropertyGrid and LXUI::EndPropertyGrid.
    // Returns true if the user changed a value. The caller then has to announce the change of the
    // component, see ComponentInfo::notifyChanged.
    bool DrawFields(const std::vector<FieldInfo>& fields, void* component, Scene* scene);
}
