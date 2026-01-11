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
    
    struct TextLine
    {
        std::string Content;
        float Width;
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
        
        void SetOutlineWidth(float outlineWidth) { m_OutlineWidth = outlineWidth; }
        float GetOutlineWidth() const { return m_OutlineWidth; }
        
        void SetOutlineColor(const glm::vec4& outlineColor) { m_OutlineColor = outlineColor; }
        const glm::vec4& GetOutlineColor() const { return m_OutlineColor; }

        void SetTextAlignment(TextAlignment alignment);
        TextAlignment GetTextAlignment() const { return m_TextAlignment; }

        void SetVerticalAlignment(TextVerticalAlignment alignment);
        TextVerticalAlignment GetTextVerticalAlignment() const { return m_VAlignment; }
        
        void SetAutoWrap(bool autoWrap);
        bool GetAutoWrap() const { return m_AutoWrap; }

        void OnMeasure(UISize availableSize) override;
        void OnArrange(UIRect finalRect) override;
        void OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint) override;

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;

    private:
        void RecalculateDesiredSize();
        void UpdateLines(float wrapWidth);
        
    private:
        std::string m_Text = "New Text";
        std::shared_ptr<Font> m_Font;
        float m_FontSize = 24.0f;
        glm::vec4 m_Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        float m_OutlineWidth = 0.0f;
        glm::vec4 m_OutlineColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        TextAlignment m_TextAlignment = TextAlignment::Left;
        TextVerticalAlignment m_VAlignment = TextVerticalAlignment::Top;
        
        bool m_AutoWrap = false;
        float m_LastMeasureWidth = 0.0f;
        std::vector<TextLine> m_Lines;
    };
}

