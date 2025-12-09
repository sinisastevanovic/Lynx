#pragma once

#include "Lynx/Core.h"

namespace Lynx
{
    using EventTypeID = uint32_t;

    LX_API EventTypeID GetNextEventTypeID();

    template<typename T>
    class TypeID
    {
    public:
        static EventTypeID Get()
        {
            static EventTypeID s_ID = GetNextEventTypeID();
            return s_ID;
        }
    };
}

