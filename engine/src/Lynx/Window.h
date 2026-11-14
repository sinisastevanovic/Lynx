#pragma once

#include <string>
#include <functional>
#include <memory>

#include "Core.h"
#include "Event/Event.h"

struct GLFWwindow;

namespace Lynx
{
    struct WindowProps
    {
        std::string Title;
        uint32_t Width;
        uint32_t Height;

        WindowProps(const std::string& title = "Lynx Engine", uint32_t width = 1600, uint32_t height = 900)
            : Title(title), Width(width), Height(height) {}
    };
    
    // Position of the top-left corner of the window's content area, and the size of that area, in screen coordinates.
    struct WindowBounds
    {
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;
    };

    enum class CursorMode
    {
        Normal = 0,
        Hidden = 1,
        Locked = 2
    };
    
    class LX_API Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;
        
        Window(const WindowProps& props);
        ~Window();

        void OnUpdate();
        void SetEventCallback(const EventCallbackFn& callback);

        uint32_t GetWidth() const { return m_Data.Width; }
        uint32_t GetHeight() const { return m_Data.Height; }
        bool ShouldClose();

        void Maximize();
        bool IsMaximized() const;
        bool IsMinimized() const;

        WindowBounds GetBounds() const;

        // Moves and resizes the window. Does nothing and returns false if the bounds would put the window
        // where it cannot be reached on the monitors that are connected now.
        bool SetBounds(const WindowBounds& bounds);

        GLFWwindow* GetNativeWindow() const { return m_Window; }
        void* GetNativeWindowHandle() const;
        
        void SetCursorMode(CursorMode mode);
        CursorMode GetCursorMode() const { return m_CursorMode; }

        static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps());

    private:
        GLFWwindow* m_Window;

        struct WindowData
        {
            std::string Title;
            uint32_t Width;
            uint32_t Height;
            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
        CursorMode m_CursorMode;
    };
}