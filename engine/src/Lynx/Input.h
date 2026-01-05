#pragma once

#include "Core.h"
#include <glm/glm.hpp>

#include "KeyCodes.h"

namespace Lynx
{
    struct AxisBinding
    {
        KeyCode Positive = KeyCode::Unknown;
        KeyCode Negative = KeyCode::Unknown;
    };
    
    class LX_API Input
    {
    public:
        // Runtime input
        static void BindAction(const std::string& name, KeyCode key);
        static void BindAxis(const std::string& name, KeyCode posKey, KeyCode negKey);

        static bool GetButton(const std::string& name);
        static float GetAxis(const std::string& name);

        static std::string GetActionFromKey(KeyCode key);

    protected:
        // System input
        static bool IsKeyPressed(KeyCode keycode);
        static glm::vec2 GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();

        static void SetViewportBounds(float x, float y, float width, float height);

    private:
        friend class Window;
        static void SetKeyState(int keycode, bool pressed);
        static void SetMousePosition(double x, double y);

        static std::unordered_map<std::string, std::vector<KeyCode>> s_ActionMappings;
        static std::unordered_map<std::string, std::vector<AxisBinding>> s_AxisBindings;
        static float s_ViewportX;
        static float s_ViewportY;
        static float s_ViewportWidth;
        static float s_ViewportHeight;

        friend class Engine;
        friend class EditorLayer;
        friend class EditorCamera;
        friend class Window;
        friend class Viewport;
    };
}
