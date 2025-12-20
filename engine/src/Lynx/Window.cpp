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
    static KeyCode GLFWToEngineKeyCode(int glfwKey)
    {
        return static_cast<KeyCode>(glfwKey);
    }

    static MouseCode GLFWToEngineMouseCode(int glfwMouseButton)
    {
        return static_cast<MouseCode>(glfwMouseButton);
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
        //SetVSync(true);

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
            Input::SetMousePosition(xpos, ypos);
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
            MouseCode keyCode = GLFWToEngineMouseCode(button);

            switch (action)
            {
                case GLFW_PRESS:
                    {
                        Input::SetKeyState(button, true);
                        MouseButtonPressedEvent event(keyCode);
                        data.EventCallback(event);
                        break;
                    }
                    case GLFW_RELEASE:
                    {
                        Input::SetKeyState(button, false);
                        MouseButtonReleasedEvent event(keyCode);
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

    void Window::SetVSync(bool enabled)
    {
        if (enabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        m_Data.VSync = enabled;
    }

    bool Window::IsVSync() const
    {
        return m_Data.VSync;
    }

    bool Window::ShouldClose()
    {
        return glfwWindowShouldClose(m_Window);
    }
}
