#pragma once

#include "Event.h"

namespace Lynx
{
    class ActionEvent : public Event
    {
    public:
        const std::string& GetActionName() const { return m_ActionName; }
    protected:
        ActionEvent(const std::string& name) : m_ActionName(name) {}
        std::string m_ActionName;
    };

    class ActionPressedEvent : public ActionEvent
    {
    public:
        ActionPressedEvent(const std::string& name) : ActionEvent(name) {}
        EVENT_CLASS_TYPE(ActionPressedEvent)
    };

    class ActionReleasedEvent : public ActionEvent
    {
    public:
        ActionReleasedEvent(const std::string& name) : ActionEvent(name) {}
        EVENT_CLASS_TYPE(ActionReleasedEvent)
    };
}
