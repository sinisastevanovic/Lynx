#pragma once

#include "Event.h"

namespace Lynx
{
    class AssetReloadedEvent : public Event
    {
    public:
        AssetReloadedEvent(AssetHandle handle) : m_Handle(handle) {}
        AssetHandle GetHandle() const { return m_Handle; }
        
        EVENT_CLASS_TYPE(AssetReloadedEvent)
    private:
        AssetHandle m_Handle;
    };
}
