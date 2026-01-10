#include "UIText.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/ImGui/EditorUIHelpers.h"
#include "Lynx/Utils/TextUtils.h"

namespace Lynx
{
    UIText::UIText()
    {
        m_Name = "UIText";
        m_Font = Engine::Get().GetAssetManager().GetDefaultFont();
    }

    void UIText::SetText(const std::string& text)
    {
        if (m_Text != text)
        {
            m_Text = text;
            RecalculateDesiredSize();
            MarkDirty();
        }
    }

    void UIText::SetFont(std::shared_ptr<Font> font)
    {
        if (m_Font != font)
        {
            m_Font = font;
            RecalculateDesiredSize();
            MarkDirty();
        }
    }

    void UIText::SetFontSize(float fontSize)
    {
        if (m_FontSize != fontSize)
        {
            m_FontSize = fontSize;
            RecalculateDesiredSize();
            MarkDirty();
        }
    }

    void UIText::SetColor(const glm::vec4& color)
    {
        m_Color = color;
    }

    void UIText::SetTextAlignment(TextAlignment alignment)
    {
        if (m_TextAlignment != alignment)
        {
            m_TextAlignment = alignment;
            /*RecalculateDesiredSize();
            MarkDirty();*/
        }
    }

    void UIText::SetVerticalAlignment(TextVerticalAlignment alignment)
    {
        if (m_VAlignment != alignment)
        {
            m_VAlignment = alignment;
        }
    }

    void UIText::SetAutoWrap(bool autoWrap)
    {
        m_AutoWrap = autoWrap;
        RecalculateDesiredSize();
    }

    void UIText::RecalculateDesiredSize()
    {
        MarkDirty();
        /*UpdateLines();
        
        if (!m_Font || m_Lines.empty())
        {
            m_DesiredSize = { 0.0f, 0.0f };
            return;
        }

        float maxW = 0.0f;
        for (const auto& line : m_Lines)
            maxW = std::max(maxW, line.Width);

        float lineHeight = m_Font->GetMetrics().LineHeight * m_FontSize;
        m_DesiredSize = { maxW, m_Lines.size() * lineHeight };*/
    }

    void UIText::UpdateLines(float wrapWidth)
    {
        m_Lines.clear();
        if (!m_Font || m_Text.empty())
            return;

        if (!m_AutoWrap || wrapWidth <= 0.0f)
            wrapWidth = 1e9f;

        std::stringstream ss(m_Text);
        std::string segment;

        while (std::getline(ss, segment, '\n'))
        {
            // Now wrap 'segment' to fit wrapWidth
            std::string currentLine;
            float currentW = 0.0f;

            // Tokenize by space to preserve words?
            // Simple: Iterate words.
            std::stringstream wordStream(segment);
            std::string word;
            bool firstWord = true;
            auto spaceGlyph = m_Font->GetGlyph(' ');

            while (std::getline(wordStream, word, ' ')) // Split by space
            {
                float wordW = 0.0f;
                // Measure word
                for (size_t k = 0; k < word.size();)
                {
                    uint32_t cp = TextUtils::DecodeNextCodepoint(word, k);
                    if (auto g = m_Font->GetGlyph(cp)) wordW += g->Advance * m_FontSize;
                }

                float spaceW = 0.0f;
                if (!firstWord && spaceGlyph)
                    spaceW = spaceGlyph->Advance * m_FontSize;

                // Check fit
                if (!firstWord && (currentW + spaceW + wordW) <= wrapWidth)
                {
                    // Fits
                    currentLine += " " + word;
                    currentW += spaceW + wordW;
                }
                else
                {
                    // Doesn't fit (or first word)
                    if (!firstWord)
                    {
                        // Flush current line
                        m_Lines.push_back({currentLine, currentW});
                        currentLine = word;
                        currentW = wordW;
                    }
                    else
                    {
                        // First word determines start
                        currentLine = word;
                        currentW = wordW;
                    }
                }
                firstWord = false;
            }
            // Flush last part
            m_Lines.push_back({currentLine, currentW});
        }
    }

    void UIText::OnMeasure(UISize availableSize)
    {
        // If wrapping is ON, use available width. Else infinite.
        float targetWidth = 0.0f;

        if (m_AutoWrap)
        {
            if (m_Size.Width > 0.0f)
                targetWidth = m_Size.Width;
            else
                targetWidth = availableSize.Width;
        }

        // Re-calculate lines based on constraint
        UpdateLines(targetWidth);

        // Calculate Desired Size (Bounding Box of wrapped text)
        float maxW = 0.0f;
        for (const auto& line : m_Lines) maxW = std::max(maxW, line.Width);
        float h = m_Lines.size() * m_Font->GetMetrics().LineHeight * m_FontSize;

        m_DesiredSize = {maxW, h};
    }

    void UIText::OnArrange(UIRect finalRect)
    {
        if (m_AutoWrap && std::abs(finalRect.Width - m_LastMeasureWidth) > 1.0f)
        {
            // Width changed! Re-wrap to actual width.
            UpdateLines(finalRect.Width);
        }
        m_LastMeasureWidth = finalRect.Width;

        UIElement::OnArrange(finalRect);
    }

    void UIText::OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint)
    {
        if (!m_Font || m_Text.empty() || m_Lines.empty())
            return;

        float finalFontSize = m_FontSize * scale;
        float width = m_DesiredSize.Width * scale;
        float lineHeight = m_Font->GetMetrics().LineHeight * finalFontSize;
        float ascender = m_Font->GetMetrics().Ascender * finalFontSize;
        float descender = m_Font->GetMetrics().Descender * finalFontSize;
        float textHeight = (m_Font->GetMetrics().Ascender - m_Font->GetMetrics().Descender) * finalFontSize;

        // 1. Vertical Alignment
        float totalH = m_Lines.size() * lineHeight;
        float startY = screenRect.Y;
        if (m_VAlignment == TextVerticalAlignment::Center)
            startY += (screenRect.Height - totalH) * 0.5f;
        else if (m_VAlignment == TextVerticalAlignment::Bottom)
            startY += (screenRect.Height - totalH);

        // 2. Render Loop
        for (size_t i = 0; i < m_Lines.size(); i++)
        {
            const auto& line = m_Lines[i];

            // Horizontal Alignment (Per Line)
            float x = screenRect.X;
            float linePixelWidth = line.Width * scale; // Scaled width

            if (m_TextAlignment == TextAlignment::Center)
                x += (screenRect.Width - linePixelWidth) * 0.5f;
            else if (m_TextAlignment == TextAlignment::Right)
                x += (screenRect.Width - linePixelWidth);

            // Calculate baseline Y for this line
            float y = startY + (i * lineHeight) + ascender;

            batcher.DrawString(line.Content, m_Font, finalFontSize, {x, y}, m_Color * parentTint);
        }
    }

    void UIText::OnInspect()
    {
        UIElement::OnInspect();
        ImGui::Separator();
        ImGui::Text("Text Properties");

        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strcpy_s(buffer, sizeof(buffer), m_Text.c_str());
        if (ImGui::InputTextMultiline("Content", buffer, sizeof(buffer)))
        {
            SetText(std::string(buffer));
        }

        float size = m_FontSize;
        if (ImGui::DragFloat("Font Size", &size, 1.0f, 1.0f, 500.0f))
        {
            SetFontSize(size);
        }

        const char* alignments[] = {"Left", "Center", "Right"};
        int current = (int)m_TextAlignment;
        if (ImGui::Combo("Orientation", &current, alignments, 3))
        {
            SetTextAlignment((TextAlignment)current);
        }

        const char* vAlignments[] = {"Top", "Center", "Bottom"};
        int currentV = (int)m_VAlignment;
        if (ImGui::Combo("Vertical Text Alignment", &currentV, vAlignments, 3))
        {
            SetVerticalAlignment((TextVerticalAlignment)currentV);
        }

        ImGui::ColorEdit4("Color", &m_Color.r);

        AssetHandle fontHandle = m_Font ? m_Font->GetHandle() : AssetHandle::Null();
        if (EditorUIHelpers::DrawAssetSelection("Font", fontHandle, {AssetType::Font}))
        {
            if (fontHandle)
            {
                m_Font = Engine::Get().GetAssetManager().GetAsset<Font>(fontHandle);
            }
            else
            {
                m_Font = nullptr;
            }
        }

        bool autoWrap = m_AutoWrap;
        if (ImGui::Checkbox("Auto Wrap", &autoWrap))
        {
            SetAutoWrap(autoWrap);
        }
    }

    void UIText::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);
        outJson["Type"] = "UIText"; // Important for polymorphic l
        outJson["Text"] = m_Text;
        outJson["FontSize"] = m_FontSize;
        outJson["Color"] = m_Color;
        outJson["Alignment"] = (int)m_TextAlignment;
        outJson["VAlignment"] = (int)m_VAlignment;
        outJson["Font"] = m_Font ? m_Font->GetHandle() : AssetHandle::Null();
        outJson["AutoWrap"] = m_AutoWrap;
    }

    void UIText::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);
        if (json.contains("Text")) m_Text = json["Text"];
        if (json.contains("FontSize")) m_FontSize = json["FontSize"];
        if (json.contains("Color")) m_Color = json["Color"].get<glm::vec4>();
        if (json.contains("Alignment")) m_TextAlignment = (TextAlignment)json["Alignment"];
        if (json.contains("VAlignment")) m_VAlignment = (TextVerticalAlignment)json["VAlignment"];
        if (json.contains("Font"))
        {
            AssetHandle fontHandle = json["Font"].get<AssetHandle>();
            if (fontHandle)
                m_Font = Engine::Get().GetAssetManager().GetAsset<Font>(fontHandle);
            else
                m_Font = nullptr;
        }
        if (json.contains("AutoWrap")) m_AutoWrap = json["AutoWrap"];

        RecalculateDesiredSize();
    }
}
