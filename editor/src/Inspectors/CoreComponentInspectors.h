#pragma once

namespace Lynx
{
    class TypeRegistry;

    // Sets the inspectors of the engine components whose UI is more than a list of fields.
    // Every other component is drawn from its field list, see FieldInspector.
    void RegisterCoreComponentInspectors(TypeRegistry& registry);
}
