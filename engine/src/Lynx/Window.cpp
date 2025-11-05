#include "Window.h"
#include "Log.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Input.h"

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

namespace Lynx
{
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

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            // TODO: Handle GLFW_REPEAT too?
            if (action == GLFW_PRESS)
            {
                Input::SetKeyState(key, true);
            }
            else if (action == GLFW_RELEASE)
            {
                Input::SetKeyState(key, false);
            }
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xpos, double ypos)
        {
            Input::SetMousePosition(xpos, ypos);
        });

        // TODO: Other callbacks (mouse buttons, window resize, etc)
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

    bool Window::ShouldClose()
    {
        return glfwWindowShouldClose(m_Window);
    }
}
