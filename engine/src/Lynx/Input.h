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
        // Returns the current mouse position. In editor, returns the mouse position in viewport space
        static glm::vec2 GetMousePosition();
        static glm::vec2 GetMouseDelta();
        static void SetCursorMode(CursorMode mode);
        static CursorMode GetCursorMode();

        static std::string GetActionFromKey(KeyCode key);
        
        static glm::vec2 GetViewportOffset() { return s_ViewPortOffset; }
        
    protected:
        // System input
        static bool IsKeyPressed(KeyCode keycode);
        static float GetMouseX();
        static float GetMouseY();

    private:
        friend class Window;
        static void SetKeyState(int keycode, bool pressed);
        static void SetMousePositionInternal(double x, double y);
        static void SetViewportOffset(float x, float y) { s_ViewPortOffset = { x, y };}
        
        static void OnUpdate();

        static std::unordered_map<std::string, std::vector<KeyCode>> s_ActionMappings;
        static std::unordered_map<std::string, std::vector<AxisBinding>> s_AxisBindings;
        static glm::vec2 s_ViewPortOffset;

        friend class Engine;
        friend class EditorLayer;
        friend class EditorCamera;
        friend class Window;
        friend class Viewport;
    };
}
