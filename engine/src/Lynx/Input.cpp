#include "Input.h"

#define MAX_KEYS 1024

namespace Lynx
{
    struct InputState
    {
        bool Keys[MAX_KEYS] = { false };
        double MouseX = 0.0f, MouseY = 0.0f;
    };

    static InputState s_State;
    
    bool Input::IsKeyPressed(int keycode)
    {
        if (keycode >= MAX_KEYS)
            return false;
        return s_State.Keys[keycode];
    }

    glm::vec2 Input::GetMousePosition()
    {
        return { s_State.MouseX, s_State.MouseY };
    }

    float Input::GetMouseX()
    {
        return s_State.MouseX;
    }

    float Input::GetMouseY()
    {
        return s_State.MouseY;
    }

    void Input::SetKeyState(int keycode, bool pressed)
    {
        if (keycode < MAX_KEYS)
        {
            s_State.Keys[keycode] = pressed;
        }
    }

    void Input::SetMousePosition(double x, double y)
    {
        s_State.MouseX = x;
        s_State.MouseY = y;
    }
}
