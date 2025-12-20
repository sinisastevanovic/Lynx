#include "EventTypeID.h"

namespace Lynx
{
    static EventTypeID s_EventTypeCounter = 0;

    LX_API EventTypeID GetNextEventTypeID()
    {
        return s_EventTypeCounter++;
    }
}