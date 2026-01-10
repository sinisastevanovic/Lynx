#include "UICanvas.h"

#include <imgui.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <nlohmann/json.hpp>

#include "Lynx/Event/Event.h"
#include "Lynx/Event/KeyEvent.h"
#include "Lynx/Event/MouseEvent.h"

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

    void UICanvas::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e)
        {
            glm::vec2 mousePos = { e.GetX(), e.GetY() }; // TODO: convert to viewport relative
            glm::vec2 canvasPos = mousePos / m_ScaleFactor;
            
            // Find what we are hovering over
            auto target = FindElementAt(canvasPos);
            auto current = m_HoveredElement.lock();
            
            // State Change: Mouse moved from A to B
            if (target != current)
            {
                if (current)
                {
                    current->m_IsMouseOver = false;
                    current->OnMouseLeave();
                }
                if (target)
                {
                    target->m_IsMouseOver = true;
                    target->OnMouseEnter();
                }
                m_HoveredElement = target;
            }
            return false;
        });
        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e)
        {
            auto target = m_HoveredElement.lock();
            while (target)
            {
                if (target->m_IsHitTestVisible && target->GetVisibility() == UIVisibility::Visible)
                {
                    if (target->OnMouseScroll(e.GetXOffset(), e.GetYOffset()))
                        return true;
                }
                target = target->GetParent();
            }
            return false;
        });
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)
        {
            if (e.GetKeyCode() != KeyCode::MouseButtonLeft)
                return false;

            auto target = m_HoveredElement.lock();
            if (target)
            {
                target->m_IsPressed = true;
                target->OnMouseDown();
                m_PressedElement = target;
                return true;
            }
            return false;
        });
        dispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& e)
        {
            if (e.GetKeyCode() != KeyCode::MouseButtonLeft)
                return false;

            auto pressed = m_PressedElement.lock();
            if (pressed)
            {
                pressed->m_IsPressed = false;
                pressed->OnMouseUp();

                auto hovered = m_HoveredElement.lock();
                if (pressed == hovered && pressed->IsEnabled())
                {
                    pressed->OnClick();
                }
                m_PressedElement.reset();
                return true;
            }
            return false;
        });
    }

    std::shared_ptr<UIElement> UICanvas::FindElementAt(const glm::vec2& point)
    {
        return FindElementRecursive(shared_from_this(), point);
    }

    std::shared_ptr<UIElement> UICanvas::FindElementRecursive(std::shared_ptr<UIElement> current, const glm::vec2& point)
    {
        if (current->GetVisibility() != UIVisibility::Visible)
            return nullptr;
        
        if (current->GetClipChildren())
        {
            if (!current->HitTest(point, true))
                return nullptr;
        }

        const auto& children = current->GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            auto child = *it;
            auto result = FindElementRecursive(child, point);
            if (result)
                return result;
        }

        if (current->HitTest(point))
            return current;

        return nullptr;
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
