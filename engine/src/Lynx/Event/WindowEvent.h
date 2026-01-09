#pragma once

#include "Lynx/Core.h"
#include "Event.h"

namespace Lynx
{
    class LX_API WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : m_Width(width), m_Height(height) {}

        inline unsigned int GetWidth() const { return m_Width; }
        inline unsigned int GetHeight() const { return m_Height; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResizeEvent)
    private:
        unsigned int m_Width, m_Height;
    };

    class LX_API WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() {}
        EVENT_CLASS_TYPE(WindowCloseEvent)
    };
    
    class LX_API ViewportResizeEvent : public Event
    {
    public:
        ViewportResizeEvent(unsigned int width, unsigned int height)
            : m_Width(width), m_Height(height) {}
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "ViewportResizeEvent: " << m_Width << ", " << m_Height;
            return ss.str();
        }
        
        EVENT_CLASS_TYPE(ViewportResizeEvent)
    private:
        uint32_t m_Width, m_Height;
    };
}