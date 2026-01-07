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
        //m_Size = { 0.0f, 0.0f };

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

            UISize childSize = child->GetSize();

            if (m_Orientation == Orientation::Vertical)
            {
                currentMainAxis += childSize.Height;
                maxCrossAxis = std::max(maxCrossAxis, childSize.Width);
            }
            else
            {
                currentMainAxis += childSize.Width;
                maxCrossAxis = std::max(maxCrossAxis, childSize.Height);
            }
        }

        if (m_Orientation == Orientation::Vertical)
            m_DesiredSize = { maxCrossAxis, currentMainAxis };
        else
            m_DesiredSize = { currentMainAxis, maxCrossAxis };
    }

    void StackPanel::OnArrange(UIRect finalRect)
    {
        m_CachedRect = finalRect;
        m_IsLayoutDirty = false;

        float currentPos = 0.0f;
        bool first = true;
        
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() == UIVisibility::Collapsed)
                continue;

            if (!first)
                currentPos += m_Spacing;
            first = false;

            UISize childSize = child->GetSize();
            UIRect childRect;


            if (m_Orientation == Orientation::Vertical)
            {
                childRect.Y = finalRect.Y + currentPos;
                childRect.Height = childSize.Height;

                float availableWidth = finalRect.Width;

                switch (child->GetHorizontalAlignment())
                {
                    case UIAlignment::Start:
                        childRect.X = finalRect.X;
                        childRect.Width = childSize.Width;
                        break;
                    case UIAlignment::Center:
                        childRect.X = finalRect.X + (availableWidth - childSize.Width) * 0.5f;
                        childRect.Width = childSize.Width;
                        break;
                    case UIAlignment::End:
                        childRect.X = finalRect.X + (availableWidth - childSize.Width);
                        childRect.Width = childSize.Width;
                        break;
                    case UIAlignment::Stretch:
                        childRect.X = finalRect.X;
                        childRect.Width = availableWidth;
                        break;
                }

                currentPos += childRect.Height;
            }
            else
            {
                childRect.X = finalRect.X + currentPos;
                childRect.Width = childSize.Width;

                float availableHeight = finalRect.Height;

                switch (child->GetVerticalAlignment())
                {
                    case UIAlignment::Start:
                        childRect.Y = finalRect.Y;
                        childRect.Height = childSize.Height;
                        break;
                    case UIAlignment::Center:
                        childRect.Y = finalRect.Y + (availableHeight - childSize.Height) * 0.5f;
                        childRect.Height = childSize.Height;
                        break;
                    case UIAlignment::End:
                        childRect.Y = finalRect.Y + (availableHeight - childSize.Height);
                        childRect.Height = childSize.Height;
                        break;
                    case UIAlignment::Stretch:
                        childRect.Y = finalRect.Y;
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
