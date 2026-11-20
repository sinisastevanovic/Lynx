#include "Window.h"
#include "Log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Input.h"
#include "KeyCodes.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include "Event/WindowEvent.h"

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

namespace Lynx
{
    namespace
    {
        // A window can be grabbed and moved if its top edge is on a monitor and enough of it is visible there.
        bool IsReachableOnAMonitor(const WindowBounds& bounds)
        {
            const int minimumVisibleSize = 100;
            if (bounds.Width < minimumVisibleSize || bounds.Height < minimumVisibleSize)
                return false;

            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            for (int i = 0; i < monitorCount; ++i)
            {
                int areaX, areaY, areaWidth, areaHeight;
                glfwGetMonitorWorkarea(monitors[i], &areaX, &areaY, &areaWidth, &areaHeight);

                const int visibleWidth = std::min(bounds.X + bounds.Width, areaX + areaWidth) - std::max(bounds.X, areaX);
                const bool topEdgeIsOnMonitor = bounds.Y >= areaY && bounds.Y <= areaY + areaHeight - minimumVisibleSize;

                if (visibleWidth >= minimumVisibleSize && topEdgeIsOnMonitor)
                    return true;
            }

            return false;
        }
    }

    static KeyCode GLFWToEngineKeyCode(int glfwKey)
    {
        return static_cast<KeyCode>(glfwKey);
    }

    static KeyCode GLFWToEngineMouseCode(int glfwMouseButton)
    {
        return static_cast<KeyCode>(glfwMouseButton + 1000);
    }
    
    static bool s_GLFWInitialized = false;

    static void GLFWErrorCallback(int error, const char* description)
    {
        LX_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    std::unique_ptr<Window> Window::Create(const WindowProps& props)
    {
        return std::make_unique<Window>(props);
    }

    void* Window::GetNativeWindowHandle() const
    {
#if defined(_WIN32)
        return glfwGetWin32Window(m_Window);
#elif defined(__linux__)
        return nullptr;
#else
        return nullptr;
#endif
    }

    void Window::SetCursorMode(CursorMode mode)
    {
        int glfwMode = GLFW_CURSOR_NORMAL;
        switch (mode)
        {
            case CursorMode::Normal: glfwMode = GLFW_CURSOR_NORMAL; break;
            case CursorMode::Hidden: glfwMode = GLFW_CURSOR_HIDDEN; break;
            case CursorMode::Locked: glfwMode = GLFW_CURSOR_DISABLED; break;
        }
        
        m_CursorMode = mode;
        glfwSetInputMode(m_Window, GLFW_CURSOR, glfwMode);
    }

    Window::Window(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        LX_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

        if (!s_GLFWInitialized)
        {
            int success = glfwInit();
            LX_ASSERT(success, "Failed to initialize GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);
            s_GLFWInitialized = true;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Window = glfwCreateWindow(static_cast<int>(m_Data.Width), static_cast<int>(m_Data.Height), m_Data.Title.c_str(), nullptr, nullptr);
        LX_ASSERT(m_Window, "Failed to create GLFW window!");

        glfwSetWindowUserPointer(m_Window, &m_Data);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;

            WindowResizeEvent event(width, height);
            data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            KeyCode keyCode = GLFWToEngineKeyCode(key);

            switch (action)
            {
                case GLFW_PRESS:
                {
                    Input::SetKeyState(key, true);
                    KeyPressedEvent event(keyCode, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    Input::SetKeyState(key, false);
                    KeyReleasedEvent event(keyCode);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(keyCode, 1);
                    data.EventCallback(event);
                    break;
                }
            }
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xpos, double ypos)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            Input::SetMousePositionInternal(xpos, ypos);
            MouseMovedEvent event(xpos, ypos);
            data.EventCallback(event);
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xoffset, double yoffset)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseScrolledEvent event(xoffset, yoffset);
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            KeyCode keyCode = GLFWToEngineMouseCode(button);

            switch (action)
            {
                case GLFW_PRESS:
                    {
                        Input::SetKeyState((int)keyCode, true);
                        KeyPressedEvent event(keyCode, 0);
                        data.EventCallback(event);
                        break;
                    }
                    case GLFW_RELEASE:
                    {
                        Input::SetKeyState((int)keyCode, false);
                        KeyReleasedEvent event(keyCode);
                        data.EventCallback(event);
                        break;
                    }
                }
        });

        // TODO: Other callbacks (window resize, etc)
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void Window::OnUpdate()
    {
        glfwPollEvents();
    }

    void Window::SetEventCallback(const EventCallbackFn& callback)
    {
        m_Data.EventCallback = callback;
    }

    bool Window::ShouldClose()
    {
        return glfwWindowShouldClose(m_Window);
    }

    void Window::Maximize()
    {
        glfwMaximizeWindow(m_Window);
    }

    bool Window::IsMaximized() const
    {
        return glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    bool Window::IsMinimized() const
    {
        return glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED) == GLFW_TRUE;
    }

    WindowBounds Window::GetBounds() const
    {
        WindowBounds bounds;
        glfwGetWindowPos(m_Window, &bounds.X, &bounds.Y);
        glfwGetWindowSize(m_Window, &bounds.Width, &bounds.Height);
        return bounds;
    }

    bool Window::SetBounds(const WindowBounds& bounds)
    {
        if (!IsReachableOnAMonitor(bounds))
            return false;

        glfwSetWindowPos(m_Window, bounds.X, bounds.Y);
        glfwSetWindowSize(m_Window, bounds.Width, bounds.Height);
        return true;
    }

    void Window::SetSize(uint32_t width, uint32_t height)
    {
        glfwSetWindowSize(m_Window, static_cast<int>(width), static_cast<int>(height));
    }
}
