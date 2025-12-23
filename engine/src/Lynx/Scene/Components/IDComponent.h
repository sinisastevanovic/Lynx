#pragma once
#include "Lynx/UUID.h"

namespace Lynx
{
    struct IDComponent
    {
        UUID ID;
        IDComponent() = default;
        IDComponent(const UUID& id) : ID(id) {}
        IDComponent(const IDComponent&) = default;
    };
}
