#include "UIButton.h"

#include <imgui.h>

namespace Lynx
{
    UIButton::UIButton()
    {
        m_Name = "Button";
        m_Size = { 120.0f, 40.0f };
        m_Tint = { 0.6f, 0.6f, 0.6f, 1.0f };
        SetHitTestVisible(true);
    }

    void UIButton::SetNormalColor(const glm::vec4& normalColor)
    {
        m_NormalColor = normalColor;
    }

    void UIButton::SetHoveredColor(const glm::vec4& hoveredColor)
    {
        m_HoveredColor = hoveredColor;
    }

    void UIButton::SetPressedColor(const glm::vec4& pressedColor)
    {
        m_PressedColor = pressedColor;
    }

    void UIButton::SetDisabledColor(const glm::vec4& disabledColor)
    {
        m_DisabledColor = disabledColor;
    }

    void UIButton::SetInteractable(bool interactable)
    {
        // TODO: Propagate to children! (Text should be greyed out etc..)
        m_Interactable = interactable;
    }

    glm::vec4 UIButton::GetTint() const
    {
        if (!m_Interactable)
            return m_DisabledColor * m_Tint;
        if (IsPressed())
            return m_PressedColor * m_Tint;
        if (IsMouseOver())
            return m_HoveredColor * m_Tint;

        return m_NormalColor * m_Tint;
    }

    void UIButton::OnMouseEnter()
    {
    }

    void UIButton::OnMouseLeave()
    {
    }

    void UIButton::OnMouseDown()
    {
    }

    void UIButton::OnMouseUp()
    {
    }

    void UIButton::OnClick()
    {
        if (m_Interactable && OnClickEvent)
            OnClickEvent();
    }

    void UIButton::OnInspect()
    {
        UIImage::OnInspect();

        ImGui::Separator();
        ImGui::Text("Button Interaction");

        ImGui::ColorEdit4("Normal Color", &m_NormalColor.r);
        ImGui::ColorEdit4("Hover Color", &m_HoveredColor.r);
        ImGui::ColorEdit4("Pressed Color", &m_PressedColor.r);
        ImGui::ColorEdit4("Disabled Color", &m_DisabledColor.r);
        ImGui::Checkbox("Interactable", &m_Interactable);
    }

    void UIButton::Serialize(nlohmann::json& outJson) const
    {
        UIImage::Serialize(outJson);
        outJson["Type"] = "UIButton";
        outJson["BtnNormal"] = m_NormalColor;
        outJson["BtnHovered"] = m_HoveredColor;
        outJson["BtnPressed"] = m_PressedColor;
        outJson["BtnDisabled"] = m_DisabledColor;
        outJson["Interactable"] = m_Interactable;
    }

    void UIButton::Deserialize(const nlohmann::json& inJson)
    {
        UIImage::Deserialize(inJson);
        m_NormalColor = inJson["BtnNormal"].get<glm::vec4>();
        m_HoveredColor = inJson["BtnHovered"].get<glm::vec4>();
        m_PressedColor = inJson["BtnPressed"].get<glm::vec4>();
        m_DisabledColor = inJson["BtnDisabled"].get<glm::vec4>();
        m_Interactable = inJson["Interactable"].get<bool>();
    }

    
}
