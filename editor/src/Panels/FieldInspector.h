#pragma once

#include <Lynx/Reflection/Field.h>

namespace Lynx
{
    class Scene;

    // Draws one inspector row per field of a component, and writes edits back into the component.
    // Call it between LXUI::BeginPropertyGrid and LXUI::EndPropertyGrid.
    void DrawFields(const std::vector<FieldInfo>& fields, void* component, Scene* scene);
}
