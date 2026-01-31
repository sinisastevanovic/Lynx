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
    
    glm::vec2 Input::s_ViewPortOffset = { 0.0f, 0.0f };

    static InputState s_State;
    static glm::vec2 s_MouseDelta = { 0, 0 };

    void Input::OnUpdate()
    {
        s_MouseDelta = { 0, 0 };
    }
    
    void Input::BindAction(const std::string& name, KeyCode key)
    {
        s_ActionMappings[name].push_back(key); // TODO: Support more than one key for a action.
    }

    void Input::BindAxis(const std::string& name, KeyCode posKey, KeyCode negKey)
    {
        s_AxisBindings[name].push_back({posKey, negKey});
    }

    bool Input::GetButton(const std::string& name)
    {
        if (Engine::Get().AreEventsBlocked() || Engine::Get().IsPaused()) // TODO: Maybe add a "Execute while Paused" for the actions
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
        if (Engine::Get().AreEventsBlocked() || Engine::Get().IsPaused())
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
        return { s_State.MouseX - s_ViewPortOffset.x, s_State.MouseY - s_ViewPortOffset.y };
    }

    glm::vec2 Input::GetMouseDelta()
    {
        if (Engine::Get().AreEventsBlocked() || Engine::Get().IsPaused())
            return { 0.0f, 0.0f };
        return s_MouseDelta;
    }

    void Input::SetCursorMode(CursorMode mode)
    {
        Engine::Get().GetWindow().SetCursorMode(mode);
    }

    CursorMode Input::GetCursorMode()
    {
        return Engine::Get().GetWindow().GetCursorMode();
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

    void Input::SetMousePositionInternal(double x, double y)
    {
        double dx = x - s_State.MouseX;
        double dy = y - s_State.MouseY;
        
        s_State.MouseX = x;
        s_State.MouseY = y;
        
        s_MouseDelta.x += (float)dx;
        s_MouseDelta.y += (float)dy;
    }
}
