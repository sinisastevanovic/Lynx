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

    void UIText::RecalculateDesiredSize()
    {
        if (!m_Font)
        {
            m_DesiredSize = { 0.0f, 0.0f };
            return;
        }

        float width = 0.0f;
        size_t i = 0;
        while (i < m_Text.size())
        {
            uint32_t cp = TextUtils::DecodeNextCodepoint(m_Text, i);
            auto g = m_Font->GetGlyph(cp);
            if (g)
                width += g->Advance;
        }

        m_DesiredSize.Width = width * m_FontSize;
        m_DesiredSize.Height = m_Font->GetMetrics().LineHeight * m_FontSize;
    }

    void UIText::OnMeasure(UISize availableSize)
    {
        RecalculateDesiredSize();
    }

    void UIText::OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint)
    {
        if (!m_Font || m_Text.empty())
            return;

        float finalFontSize = m_FontSize * scale;
        float width = m_DesiredSize.Width * scale;
        float ascender = m_Font->GetMetrics().Ascender * finalFontSize;
        float descender = m_Font->GetMetrics().Descender * finalFontSize;
        float textHeight = (m_Font->GetMetrics().Ascender - m_Font->GetMetrics().Descender) * finalFontSize;

        float x = screenRect.X;
        if (m_TextAlignment == TextAlignment::Center)
            x = screenRect.X + (screenRect.Width - width) * 0.5f;
        else if (m_TextAlignment == TextAlignment::Right)
            x = screenRect.X + (screenRect.Width - width);

        float y = screenRect.Y;
        switch (m_VAlignment)
        {
            case TextVerticalAlignment::Top:
                y += ascender;
                break;
            case TextVerticalAlignment::Center:
                y += (screenRect.Height - textHeight) * 0.5f + ascender;
                break;
            case TextVerticalAlignment::Bottom:
                y += screenRect.Height + descender;
        }

        batcher.DrawString(m_Text, m_Font, finalFontSize, { x, y }, m_Color * parentTint);
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

        const char* alignments[] = { "Left", "Center", "Right" };
        int current = (int)m_TextAlignment;
        if (ImGui::Combo("Orientation", &current, alignments, 3))
        {
            SetTextAlignment((TextAlignment)current);
        }

        const char* vAlignments[] = { "Top", "Center", "Bottom" };
        int currentV = (int)m_VAlignment;
        if (ImGui::Combo("Vertical Text Alignment", &currentV, vAlignments, 3))
        {
            SetVerticalAlignment((TextVerticalAlignment)currentV);
        }

        ImGui::ColorEdit4("Color", &m_Color.r);

        AssetHandle fontHandle = m_Font ? m_Font->GetHandle() : AssetHandle::Null();
        if (EditorUIHelpers::DrawAssetSelection("Font", fontHandle, { AssetType::Font }))
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

        RecalculateDesiredSize();
    }

    
}
