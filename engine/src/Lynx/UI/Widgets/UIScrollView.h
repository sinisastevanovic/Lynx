#pragma once
#include "Lynx/UI/Core/UIElement.h"

namespace Lynx
{
    class LX_API UIScrollView : public UIElement
    {
    public:
        UIScrollView();
        virtual ~UIScrollView() override = default;
        
        void SetScrollPosition(const glm::vec2& pos);
        glm::vec2 GetScrollPosition() const { return m_ScrollPosition; }
        
        void SetVerticalScrollbar(std::shared_ptr<UIElement> verticalScrollbar);
        std::shared_ptr<UIElement> GetVerticalScrollbar() const { return m_VerticalScrollbar; }
        
        
        bool OnMouseScroll(float offsetX, float offsetY) override;
        void OnMeasure(UISize availableSize) override;
        void OnArrange(UIRect finalRect) override;
        
        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;
        
    private:
        void UpdateScrollBarState();
        
    private:
        glm::vec2 m_ScrollPosition = { 0, 0 };
        float m_ScrollSensitivity = 20.0f;
        
        UISize m_ContentSize = { 0, 0 };
        
        std::shared_ptr<UIElement> m_VerticalScrollbar;
    };

}
