#include "StackPanel.h"

#include <imgui.h>
#include <glm/gtc/epsilon.hpp>

namespace Lynx
{
    StackPanel::StackPanel()
    {
        m_Name = "StackPanel";
    }

    void StackPanel::OnMeasure(UISize availableSize)
    {
        float currentMainAxis = 0.0f;
        float maxCrossAxis = 0.0f;

        bool first = true;
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() == UIVisibility::Collapsed)
                continue;

            if (!first)
                currentMainAxis += m_Spacing;
            first = false;

            UISize childAvailable = availableSize;
            if (m_Orientation == Orientation::Vertical)
                childAvailable.Height = 99999.0f;
            else
                childAvailable.Width = 99999.0f;

            child->OnMeasure(childAvailable);

            UISize explicitChildSize = child->GetSize();
            UISize desiredChildSize = child->GetDesiredSize();
            
            float childW = (glm::abs(explicitChildSize.Width) > 0.001f) ? explicitChildSize.Width : desiredChildSize.Width;
            float childH = (glm::abs(explicitChildSize.Height) > 0.001f) ? explicitChildSize.Height : desiredChildSize.Height;

            if (m_Orientation == Orientation::Vertical)
            {
                currentMainAxis += childH;
                maxCrossAxis = std::max(maxCrossAxis, childW);
            }
            else
            {
                currentMainAxis += childW;
                maxCrossAxis = std::max(maxCrossAxis, childH);
            }
        }

        if (m_Orientation == Orientation::Vertical)
        {
            m_DesiredSize.Width = maxCrossAxis + m_Padding.Left + m_Padding.Right;
            m_DesiredSize.Height = currentMainAxis + m_Padding.Top + m_Padding.Bottom;
        }
        else
        {
            
            m_DesiredSize.Width = currentMainAxis + m_Padding.Left + m_Padding.Right;
            m_DesiredSize.Height = maxCrossAxis + m_Padding.Top + m_Padding.Bottom;
        }
    }

    void StackPanel::OnArrange(UIRect finalRect)
    {
        m_CachedRect = finalRect;
        m_IsLayoutDirty = false;

        float currentPos = (m_Orientation == Orientation::Vertical) ? m_Padding.Top : m_Padding.Left;
        bool first = true;
        
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() == UIVisibility::Collapsed)
                continue;

            if (!first)
                currentPos += m_Spacing;
            first = false;

            UISize childSize = child->GetSize();
            UISize childDesiredSize = child->GetDesiredSize();
            UISize effectiveSize = {
                glm::abs(childSize.Width) > 0.001f ? childSize.Width : childDesiredSize.Width,
                glm::abs(childSize.Height) > 0.001f ? childSize.Height : childDesiredSize.Height,
            };
            
            UIRect childRect;

            if (m_Orientation == Orientation::Vertical)
            {
                childRect.Y = finalRect.Y + currentPos;
                childRect.Height = effectiveSize.Height;

                float availableWidth = finalRect.Width - (m_Padding.Left + m_Padding.Right);
                float startX = finalRect.X + m_Padding.Left;

                switch (child->GetHorizontalAlignment())
                {
                    case UIAlignment::Start:
                        childRect.X = startX;
                        childRect.Width = effectiveSize.Width;
                        break;
                    case UIAlignment::Center:
                        childRect.X = startX + (availableWidth - effectiveSize.Width) * 0.5f;
                        childRect.Width = effectiveSize.Width;
                        break;
                    case UIAlignment::End:
                        childRect.X = startX + (availableWidth - effectiveSize.Width);
                        childRect.Width = effectiveSize.Width;
                        break;
                    case UIAlignment::Stretch:
                        childRect.X = startX;
                        childRect.Width = availableWidth;
                        break;
                }

                currentPos += childRect.Height;
            }
            else
            {
                childRect.X = finalRect.X + currentPos;
                childRect.Width = effectiveSize.Width;

                float availableHeight = finalRect.Height - (m_Padding.Top + m_Padding.Bottom);
                float startY = finalRect.Y + m_Padding.Top;

                switch (child->GetVerticalAlignment())
                {
                    case UIAlignment::Start:
                        childRect.Y = startY;
                        childRect.Height = effectiveSize.Height;
                        break;
                    case UIAlignment::Center:
                        childRect.Y = startY + (availableHeight - effectiveSize.Height) * 0.5f;
                        childRect.Height = effectiveSize.Height;
                        break;
                    case UIAlignment::End:
                        childRect.Y = startY + (availableHeight - effectiveSize.Height);
                        childRect.Height = effectiveSize.Height;
                        break;
                    case UIAlignment::Stretch:
                        childRect.Y = startY;
                        childRect.Height = availableHeight;
                        break;
                }
                currentPos += childRect.Width;
            }
            child->OnArrange(childRect);
        }
    }

    void StackPanel::SetOrientation(Orientation orientation)
    {
        if (m_Orientation == orientation)
            return;
        m_Orientation = orientation;
        MarkDirty();
    }

    void StackPanel::SetSpacing(float spacing)
    {
        if (glm::epsilonNotEqual(m_Spacing, spacing, glm::epsilon<float>()))
        {
            m_Spacing = spacing;
            MarkDirty();
        }
    }

    void StackPanel::OnInspect()
    {
        UIElement::OnInspect();
        ImGui::Separator();
        ImGui::Text("Stack Layout");

        const char* orientations[] = { "Vertical", "Horizontal" };
        int current = (int)m_Orientation;
        if (ImGui::Combo("Orientation", &current, orientations, 2))
        {
            SetOrientation((Orientation)current);
        }

        float spacing = m_Spacing;
        if (ImGui::DragFloat("Spacing", &spacing))
        {
            SetSpacing(spacing);
        }
    }

    void StackPanel::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);
        outJson["Type"] = "StackPanel";
        outJson["Orientation"] = (int)m_Orientation;
        outJson["Spacing"] = m_Spacing;
    }

    void StackPanel::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);
        if (json.contains("Orientation"))
            m_Orientation = (Orientation)json["Orientation"];
        if (json.contains("Spacing"))
            m_Spacing = json["Spacing"];
    }
}
