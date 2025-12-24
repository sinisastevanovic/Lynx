#pragma once

#include "Lynx/Core.h"
#include "Event.h"
#include "Lynx/KeyCodes.h"
#include <sstream>

namespace Lynx
{
    class LX_API MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const float x, const float y)
            : m_MouseX(x), m_MouseY(y) {}

        float GetX() const { return m_MouseX; }
        float GetY() const { return m_MouseY; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMovedEvent)

    private:
        float m_MouseX, m_MouseY;
    };

    class LX_API MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(const float xOffset, const float yOffset)
            : m_XOffset(xOffset), m_YOffset(yOffset) {}

        float GetXOffset() const { return m_XOffset; }
        float GetYOffset() const { return m_YOffset; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolledEvent)

    private:
        float m_XOffset, m_YOffset;
    };
}