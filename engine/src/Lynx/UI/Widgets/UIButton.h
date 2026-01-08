#pragma once
#include "UIImage.h"

namespace Lynx
{
    class LX_API UIButton : public UIImage
    {
    public:
        UIButton();
        ~UIButton() override = default;

        void SetNormalColor(const glm::vec4& normalColor);
        void SetHoveredColor(const glm::vec4& hoveredColor);
        void SetPressedColor(const glm::vec4& pressedColor);
        void SetDisabledColor(const glm::vec4& disabledColor);
        glm::vec4 GetTint() const override;

        std::function<void()> OnClickEvent;

        void OnMouseEnter() override;
        void OnMouseLeave() override;
        void OnMouseDown() override;
        void OnMouseUp() override;
        void OnClick() override;

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& inJson) override;

    private:
        glm::vec4 m_NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 m_HoveredColor = { 0.8f, 0.8f, 0.8f, 1.0f };
        glm::vec4 m_PressedColor = { 0.6f, 0.6f, 0.6f, 1.0f };
        glm::vec4 m_DisabledColor = { 0.4f, 0.4f, 0.4f, 1.0f };
    };

}
