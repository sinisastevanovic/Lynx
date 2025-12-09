#pragma once

#include "Core.h"
#include <glm/glm.hpp>

#include "KeyCodes.h"

namespace Lynx
{
    // TODO: Weird class, input state should be more sophisticated and passing GLFW keycodes sucks.
    class LX_API Input
    {
    public:
        static bool IsKeyPressed(KeyCode keycode);
        static bool IsMouseButtonPressed(MouseCode button);
        static glm::vec2 GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();

    private:
        friend class Window;
        static void SetKeyState(int keycode, bool pressed);
        static void SetMousePosition(double x, double y);
    };
}
