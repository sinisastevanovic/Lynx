#include "GridPanel.h"

#include <algorithm>
#include <imgui.h>

namespace Lynx
{
    GridPanel::GridPanel()
    {
        m_Name = "GridPanel";
    }

    void GridPanel::SetCellSize(const glm::vec2& size)
    {
        m_CellSize = size;
        MarkDirty();
    }

    void GridPanel::SetSpacing(const glm::vec2& spacing)
    {
        m_Spacing = spacing;
        MarkDirty();
    }

    void GridPanel::SetConstraint(int columns)
    {
        m_ConstraintCount = columns;
        MarkDirty();
    }

    void GridPanel::OnMeasure(UISize availableSize)
    {
        int count = 0;
        for (auto& c : m_Children)
        {
            if (c->GetVisibility() != UIVisibility::Collapsed)
            {
                c->OnMeasure(availableSize);
                count++;
            }
        }
        
        if (count == 0)
        {
            m_DesiredSize = { 0, 0 };
            return;
        }
        
        int cols = m_ConstraintCount;
        cols = std::max(cols, 1);

        int rows = (count + cols - 1) / cols;
        
        float width = cols * m_CellSize.x + (std::max(0, cols - 1) * m_Spacing.x);
        float height = rows * m_CellSize.y + (std::max(0, rows - 1) * m_Spacing.y);
        
        m_DesiredSize.Width = width + m_Padding.Left + m_Padding.Right;
        m_DesiredSize.Height = height + m_Padding.Top + m_Padding.Bottom;
    }

    void GridPanel::OnArrange(UIRect finalRect)
    {
        m_CachedRect = finalRect;
        
        float startX = finalRect.X + m_Padding.Left;
        float startY = finalRect.Y + m_Padding.Top;
        
        int cols = m_ConstraintCount;
        cols = std::max(cols, 1);

        int visibleIndex = 0;
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() == UIVisibility::Collapsed)
                continue;
            
            int c = visibleIndex % cols;
            int r = visibleIndex / cols;
            
            UIRect childRect;
            childRect.X = startX + c * (m_CellSize.x + m_Spacing.x);
            childRect.Y = startY + r * (m_CellSize.y + m_Spacing.y);
            childRect.Width = m_CellSize.x;
            childRect.Height = m_CellSize.y;
            
            child->OnArrange(childRect);
            visibleIndex++;
        }
    }

    void GridPanel::OnInspect()
    {
        UIElement::OnInspect();
        
        ImGui::Separator();
        ImGui::Text("Grid Panel");
        
        float size[2] = { m_CellSize.x, m_CellSize.y };
        if (ImGui::DragFloat2("Cell Size", size, 1.0f, 0.01f, 9999))
        {
            SetCellSize({ size[0], size[1] });
        }
        
        float spacing[2] = { m_Spacing.x, m_Spacing.y };
        if (ImGui::DragFloat2("Spacing", spacing, 0.01f, 0.0f, 9999))
        {
            SetSpacing({ spacing[0], spacing[1] });
        }
        
        int constraint = m_ConstraintCount;
        if (ImGui::DragInt("Columns", &constraint, 1, 0, 9999))
        {
            SetConstraint(constraint);
        }
    }

    void GridPanel::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);
        outJson["Type"] = "GridPanel";
        outJson["CellSize"] = m_CellSize;
        outJson["Spacing"] = m_Spacing;
        outJson["Constraint"] = m_ConstraintCount;
    }

    void GridPanel::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);
        m_CellSize = json["CellSize"].get<glm::vec2>();
        m_Spacing = json["Spacing"].get<glm::vec2>();
        m_ConstraintCount = json["Constraint"];
    }
}
