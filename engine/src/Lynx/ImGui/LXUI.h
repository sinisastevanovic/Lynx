#pragma once
#include "Lynx/UUID.h"
#include "Lynx/Asset/AssetTypes.h"
#include "Lynx/Scene/Components/LuaScriptComponent.h"
#include <glm/glm.hpp>

namespace Lynx
{
    class Scene;
    class LX_API LXUI
    {
    public:
        // Core Layout
        static void BeginPropertyGrid();
        static void EndPropertyGrid();
        static void DrawLabel(const std::string& label);
        static bool PropertyGroup(const std::string& name, bool autoOpen = true);
        
        // Controls
        static bool DrawDragFloat(const std::string& label, float& value, float speed = 1.0f, float min = 0, float max = 0, float resetValue = 0.0f);
        static bool DrawSliderFloat(const std::string& label, float& value, float min = 0, float max = 0, float resetValue = 0.0f);
        static bool DrawDragInt(const std::string& label, int& value, float speed = 1.0f, int min = 0, int max = 0, int resetValue = 0);
        static bool DrawCheckBox(const std::string& label, bool& value);
        static bool DrawTextInput(const std::string& label, std::string& value);
        static bool DrawTextInputMultiline(const std::string& label, std::string& value);
        
        static bool DrawColorControl(const std::string& label, glm::vec4& value);
        static bool DrawColor3Control(const std::string& label, glm::vec3& value);
        
        static bool DrawVec2Control(const std::string& label, glm::vec2& value, float speed = 0.1f, float min = 0, float max = 0, float resetValue = 0.0f);
        static bool DrawVec3Control(const std::string& label, glm::vec3& value, float min = 0, float max = 0, float resetValue = 0.0f);
        static bool DrawVec4Control(const std::string& label, glm::vec4& value, float min = 0, float max = 0, float resetValue = 0.0f);
        
        static bool DrawComboControl(const std::string& label, int& currentItem, const std::vector<std::string>& items);
        static bool DrawAssetReference(const std::string& label, AssetHandle& currentHandle, std::initializer_list<AssetType> allowedTypes);
        static void DrawLuaScriptSection(LuaScriptComponent* lsc);
        
        // UI Specific Controls
        static bool DrawUIAnchorControl(const std::string& label, UIAnchor& anchor);
        static bool DrawUIThicknessControl(const std::string& label, UIThickness& thickness);
        static bool DrawUIElementSelection(const std::string& label, UUID& id, Scene* context);
    };
}

