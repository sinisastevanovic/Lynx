#pragma once
#include "Lynx/Asset/Font.h"
#include "Lynx/UI/Core/UIElement.h"

namespace Lynx
{
    enum class TextAlignment
    {
        Left,
        Center,
        Right
    };

    enum class TextVerticalAlignment
    {
        Top,
        Center,
        Bottom
    };
    
    class LX_API UIText : public UIElement
    {
    public:
        UIText();
        virtual ~UIText() override = default;

        void SetText(const std::string& text);
        const std::string& GetText() const { return m_Text; }

        void SetFont(std::shared_ptr<Font> font);
        std::shared_ptr<Font> GetFont() const { return m_Font; }

        void SetFontSize(float fontSize);
        float GetFontSize() const { return m_FontSize; }

        void SetColor(const glm::vec4& color);
        const glm::vec4& GetColor() const { return m_Color; }

        void SetTextAlignment(TextAlignment alignment);
        TextAlignment GetTextAlignment() const { return m_TextAlignment; }

        void SetVerticalAlignment(TextVerticalAlignment alignment);
        TextVerticalAlignment GetTextVerticalAlignment() const { return m_VAlignment; }

        void OnMeasure(UISize availableSize) override;
        void OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint) override;

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;

    private:
        void RecalculateDesiredSize();
        
    private:
        std::string m_Text = "New Text";
        std::shared_ptr<Font> m_Font;
        float m_FontSize = 24.0f;
        glm::vec4 m_Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        TextAlignment m_TextAlignment = TextAlignment::Left;
        TextVerticalAlignment m_VAlignment = TextVerticalAlignment::Top;
    };
}

