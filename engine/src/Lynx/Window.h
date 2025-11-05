#pragma once

#include <string>
#include <functional>
#include <memory>

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
    
    class Window
    {
    public:
        Window(const WindowProps& props);
        ~Window();

        void OnUpdate();

        uint32_t GetWidth() const { return m_Data.Width; }
        uint32_t GetHeight() const { return m_Data.Height; }
        bool ShouldClose();

        GLFWwindow* GetNativeWindow() const { return m_Window; }
        void* GetNativeWindowHandle() const;

        static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps());

    private:
        GLFWwindow* m_Window;

        struct WindowData
        {
            std::string Title;
            uint32_t Width;
            uint32_t Height;
        };

        WindowData m_Data;
    
    };
}