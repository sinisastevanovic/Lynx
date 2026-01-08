#pragma once
#include "UIElement.h"
#include <glm/glm.hpp>


namespace Lynx
{
    class Event;
    class MouseMovedEvent;
    class KeyPressedEvent;
    class KeyReleasedEvent;

    enum class UIScaleMode
    {
        ConstantPixelSize,
        ScaleWithScreenSize
    };
    
    class LX_API UICanvas : public UIElement
    {
    public:
        UICanvas();
        ~UICanvas() override = default;

        void Update(float deltaTime, float screenWidth, float screenHeight);

        void SetScaleMode(UIScaleMode mode);
        UIScaleMode GetScaleMode() const { return m_ScaleMode; }

        void SetReferenceResolution(glm::vec2 resolution);
        glm::vec2 GetReferenceResolution() const { return m_ReferenceResolution; }

        float GetScaleFactor() const { return m_ScaleFactor; }

        void OnEvent(Event& e);

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;

    private:
        float CalculateScaleFactor(float screenW, float screenH) const;
        std::shared_ptr<UIElement> FindElementAt(const glm::vec2& point);
        std::shared_ptr<UIElement> FindElementRecursive(std::shared_ptr<UIElement> current, const glm::vec2& point);

    private:
        UIScaleMode m_ScaleMode = UIScaleMode::ScaleWithScreenSize;
        glm::vec2 m_ReferenceResolution = { 1920.0f, 1080.0f };
        float m_ScaleFactor = 1.0f;
        float m_LastScreenWidth = 0.0f;
        float m_LastScreenHeight = 0.0f;

        std::weak_ptr<UIElement> m_HoveredElement;
        std::weak_ptr<UIElement> m_PressedElement;
    };
}

