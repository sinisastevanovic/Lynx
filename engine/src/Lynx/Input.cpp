#include "Input.h"

#include "Engine.h"

#define MAX_KEYS 1024 // TODO: Rethink this when we add gamepad support, because mouse buttons start at 1000

namespace Lynx
{
    struct InputState
    {
        bool Keys[MAX_KEYS] = { false };
        double MouseX = 0.0f, MouseY = 0.0f;
        InputState()
        {
            memset(Keys, 0, sizeof(Keys));
        }
    };

    std::unordered_map<std::string, std::vector<KeyCode>> Input::s_ActionMappings;
    std::unordered_map<std::string, std::vector<AxisBinding>> Input::s_AxisBindings;
    float Input::s_ViewportX = 0.0f;
    float Input::s_ViewportY = 0.0f;
    float Input::s_ViewportWidth = 0.0f;
    float Input::s_ViewportHeight = 0.0f;

    static InputState s_State;

    void Input::BindAction(const std::string& name, KeyCode key)
    {
        s_ActionMappings[name].push_back(key);
    }

    void Input::BindAxis(const std::string& name, KeyCode posKey, KeyCode negKey)
    {
        s_AxisBindings[name].push_back({posKey, negKey});
    }

    bool Input::GetButton(const std::string& name)
    {
        if (Engine::Get().AreEventsBlocked())
            return false;
        
        if (s_ActionMappings.find(name) != s_ActionMappings.end())
        {
            for (auto key : s_ActionMappings[name])
                if (IsKeyPressed(key))
                    return true;
        }
        return false;
    }

    float Input::GetAxis(const std::string& name)
    {
        if (Engine::Get().AreEventsBlocked())
            return 0.0f;
        
        float value = 0.0f;
        if (s_AxisBindings.find(name) != s_AxisBindings.end())
        {
            for (auto& binding : s_AxisBindings[name])
            {
                if (IsKeyPressed(binding.Positive))
                    value += 1.0f;
                if (IsKeyPressed(binding.Negative))
                    value -= 1.0f;
            }
        }
        return glm::clamp(value, -1.0f, 1.0f);
    }

    std::string Input::GetActionFromKey(KeyCode key)
    {
        for (auto& [name, keys] : s_ActionMappings)
        {
            for (auto k : keys)
            {
                if (k == key)
                    return name;
            }
        }
        return std::string();
    }

    bool Input::IsKeyPressed(KeyCode keycode)
    {
        int kc = static_cast<int>(keycode);
        if (kc >= MAX_KEYS)
            return false;
        return s_State.Keys[kc];
    }

    glm::vec2 Input::GetMousePosition()
    {
        return { (float)s_State.MouseX - s_ViewportX, (float)s_State.MouseY - s_ViewportY };
    }

    float Input::GetMouseX()
    {
        return (float)s_State.MouseX - s_ViewportX;
    }

    float Input::GetMouseY()
    {
        return (float)s_State.MouseY - s_ViewportY;
    }

    void Input::SetViewportBounds(float x, float y, float width, float height)
    {
        s_ViewportX = x;
        s_ViewportY = y;
        s_ViewportWidth = width;
        s_ViewportHeight = height;
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
