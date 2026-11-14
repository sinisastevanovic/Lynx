#pragma once

#include "Lynx/Core.h"

namespace Lynx
{
    class TypeRegistry;

    // Registers the components that are part of the engine, with how each one is saved and loaded.
    // How the editor draws them is the editor's business, see CoreComponentInspectors in the editor.
    void RegisterCoreComponents(TypeRegistry& registry);
}
