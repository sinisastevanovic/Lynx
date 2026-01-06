#include "UICanvas.h"

#include <imgui.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <nlohmann/json.hpp>

namespace Lynx
{
    UICanvas::UICanvas()
    {
        m_Name = "Canvas";
        m_Anchor = UIAnchor::StretchAll;
        m_Offset = { 0, 0 };
        m_Size = { 0, 0 };
    }

    void UICanvas::Update(float deltaTime, float screenWidth, float screenHeight)
    {
        float scale = CalculateScaleFactor(screenWidth, screenHeight);
        bool sizeChanged = glm::abs(m_LastScreenWidth - screenWidth) > 0.5f ||
                            glm::abs(m_LastScreenHeight - screenHeight) > 0.5f;
        bool scaleChanged = glm::epsilonNotEqual(m_ScaleFactor, scale, glm::epsilon<float>());
        
        if (m_IsLayoutDirty || sizeChanged || scaleChanged)
        {
            m_ScaleFactor = scale;
            m_LastScreenWidth = screenWidth;
            m_LastScreenHeight = screenHeight;

            UISize logicalSize;
            logicalSize.Width = screenWidth / m_ScaleFactor;
            logicalSize.Height = screenHeight / m_ScaleFactor;
            
            UIRect rootRect;
            rootRect.X = 0;
            rootRect.Y = 0;
            rootRect.Width = logicalSize.Width;
            rootRect.Height = logicalSize.Height;

            OnMeasure(logicalSize);
            OnArrange(rootRect);
            m_IsLayoutDirty = false;
        }
        OnUpdate(deltaTime);
    }

    float UICanvas::CalculateScaleFactor(float screenW, float screenH) const
    {
        if (m_ScaleMode == UIScaleMode::ConstantPixelSize)
            return 1.0f;

        if (m_ReferenceResolution.x <= 0)
            return 1.0f;

        return screenW / m_ReferenceResolution.x;
    }

    void UICanvas::SetScaleMode(UIScaleMode mode)
    {
        if (m_ScaleMode == mode)
            return;
        
        m_ScaleMode = mode;
        MarkDirty();
    }

    void UICanvas::SetReferenceResolution(glm::vec2 resolution)
    {
        if (m_ReferenceResolution == resolution)
            return;
        
        m_ReferenceResolution = resolution;
        MarkDirty();
    }

    void UICanvas::OnInspect()
    {
        UIElement::OnInspect();

        ImGui::Separator();
        ImGui::Text("Canvas Scaler");

        const char* modes[] = { "Constant Pixel Size", "Scale With Screen Size" };
        int currentMode = (int)m_ScaleMode;
        if (ImGui::Combo("Scale Mode", &currentMode, modes, 2))
        {
            m_ScaleMode = (UIScaleMode)currentMode;
            MarkDirty();
        }

        if (m_ScaleMode == UIScaleMode::ScaleWithScreenSize)
        {
            float res[2] = { m_ReferenceResolution.x, m_ReferenceResolution.y };
            if (ImGui::DragFloat2("Reference Res", res))
            {
                m_ReferenceResolution = { res[0], res[1] };
                MarkDirty();
            }
        }

        ImGui::Text("Current Scale Factor: %.2f", m_ScaleFactor);
    }

    void UICanvas::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);

        outJson["Type"] = "UICanvas";
        outJson["ScaleMode"] = (int)m_ScaleMode;
        outJson["RefRes"] = { m_ReferenceResolution.x, m_ReferenceResolution.y };
    }

    void UICanvas::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);

        if (json.contains("ScaleMode")) m_ScaleMode = (UIScaleMode)json["ScaleMode"];
        if (json.contains("RefRes")) {
            auto& r = json["RefRes"];
            m_ReferenceResolution = { r[0], r[1] };
        }

    }

    
}
