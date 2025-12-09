#pragma once
#include <string>
#include <Lynx/Core.h>

#include "EventTypeID.h"

namespace Lynx
{
#define EVENT_CLASS_TYPE(type)                                                 \
    static Lynx::EventTypeID GetStaticType() {                              \
        return Lynx::TypeID<type>::Get();                                   \
    }                                                                          \
    virtual Lynx::EventTypeID GetEventType() const override {               \
        return GetStaticType();                                                \
    }                                                                          \
    virtual const char* GetName() const override { return #type; }
    
    class LX_API Event
    {
        friend class EventDispatcher;
    public:
        virtual ~Event() = default;

        virtual EventTypeID GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual std::string ToString() const { return GetName(); }

        bool Handled = false;
    };

    class LX_API EventDispatcher
    {
    public:
        EventDispatcher(Event& event)
            : m_Event(event) {}

        template <typename T, typename F>
        bool Dispatch(const F& func)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.Handled = func(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }
    private:
        Event& m_Event;
    };

    inline std::ostream& operator<<(std::ostream& os, const Event& e)
    {
        return os << e.ToString();
    }
}

