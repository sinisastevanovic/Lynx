#include "UIElement.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include "Lynx/UI/Rendering/UIBatcher.h"
#include "Lynx/UI/Widgets/StackPanel.h"
#include "Lynx/UI/Widgets/UIImage.h"

namespace Lynx
{
    UIElement::UIElement()
    {
        m_Anchor = { 0.5f, 0.5f, 0.5f, 0.5f };
        m_Pivot = { 0.5f, 0.5f };
    }

    void UIElement::AddChild(std::shared_ptr<UIElement> child)
    {
        if (child->GetParent())
            child->GetParent()->RemoveChild(child);

        child->m_Parent = shared_from_this();
        m_Children.push_back(child);
        MarkDirty();
    }

    void UIElement::RemoveChild(std::shared_ptr<UIElement> child)
    {
        auto it = std::find(m_Children.begin(), m_Children.end(), child);
        if (it != m_Children.end())
        {
            child->m_Parent.reset();
            m_Children.erase(it);
            MarkDirty();
        }
    }

    void UIElement::ClearChildren()
    {
        for (auto& child : m_Children)
            child->m_Parent.reset();
        m_Children.clear();
        MarkDirty();
    }

    void UIElement::SetOffset(UIPoint offset)
    {
        if (m_Offset.X != offset.X || m_Offset.Y != offset.Y)
        {
            m_Offset = offset;
            MarkDirty();
        }
    }

    void UIElement::SetSize(UISize size)
    {
        if (m_Size.Width != size.Width || m_Size.Height != size.Height)
        {
            m_Size = size;
            MarkDirty();
        }
    }

    void UIElement::SetAnchor(UIAnchor anchor)
    {
        m_Anchor = anchor;
        MarkDirty();
    }

    void UIElement::SetPivot(glm::vec2 pivot)
    {
        m_Pivot = pivot;
        MarkDirty();
    }

    void UIElement::SetHorizontalAlignment(UIAlignment alignment)
    {
        if (alignment != m_HorizontalAlignment)
        {
            m_HorizontalAlignment = alignment;
            MarkDirty();
        }
    }

    void UIElement::SetVerticalAlignment(UIAlignment alignment)
    {
        if (alignment != m_VerticalAlignment)
        {
            m_VerticalAlignment = alignment;
            MarkDirty();
        }
    }

    void UIElement::SetVisibility(UIVisibility visibility)
    {
        if (m_Visibility == visibility)
            return;

        bool needsLayoutUpdate = (m_Visibility == UIVisibility::Collapsed || visibility == UIVisibility::Collapsed);
        m_Visibility = visibility;
        if (needsLayoutUpdate)
            MarkDirty();
    }

    void UIElement::MarkDirty()
    {
        m_IsLayoutDirty = true;
        if (auto parent = m_Parent.lock())
        {
            parent->MarkDirty();
        }
    }

    void UIElement::OnUpdate(float deltaTime)
    {
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() != UIVisibility::Collapsed)
                child->OnUpdate(deltaTime);
        }
    }

    void UIElement::OnMeasure(UISize availableSize)
    {
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() != UIVisibility::Collapsed)
            {
                child->OnMeasure(availableSize);
            }
        }
    }

    UIRect UIElement::CalculateBounds(UIRect parentRect) const
    {
        UIRect rect;

        // 1. Calculate the Anchor Box in parent space
        float parentW = parentRect.Width;
        float parentH = parentRect.Height;

        float anchorMinX = parentRect.X + (parentW * m_Anchor.MinX);
        float anchorMaxX = parentRect.X + (parentW * m_Anchor.MaxX);
        float anchorMinY = parentRect.Y + (parentH * m_Anchor.MinY);
        float anchorMaxY = parentRect.Y + (parentH * m_Anchor.MaxY);

        // 2. Resolve Size and Position based on Anchors
        if (glm::abs(m_Anchor.MinX - m_Anchor.MaxX) < 0.0001f)
        {
            // Pinned (e.g. Center): Size is explicit
            float width = m_Size.Width;
            if (glm::abs(width) < 0.001f)
                width = m_DesiredSize.Width;
            rect.Width = width;
            rect.X = anchorMinX + m_Offset.X - (rect.Width * m_Pivot.x);
        }
        else
        {
            // Stretched: Size is AnchorBox + SizeDelta (margins)
            rect.Width = (anchorMaxX - anchorMinX) + m_Size.Width;
            float center = anchorMinX + (anchorMaxX - anchorMinX) * 0.5f;
            rect.X = center + m_Offset.X - (rect.Width * m_Pivot.x);
        }

        if (glm::abs(m_Anchor.MinY - m_Anchor.MaxY) < 0.0001f)
        {
            float height = m_Size.Height;
            if (glm::abs(height) < 0.001f)
                height = m_DesiredSize.Height;
            rect.Height = height;
            rect.Y = anchorMinY + m_Offset.Y - (rect.Height * m_Pivot.y);
        }
        else
        {
            rect.Height = (anchorMaxY - anchorMinY) + m_Size.Height;
            float center = anchorMinY + (anchorMaxY - anchorMinY) * 0.5f;
            rect.Y = center + m_Offset.Y - (rect.Height * m_Pivot.y);
        }

        return rect;
    }

    void UIElement::OnArrange(UIRect finalRect)
    {
        m_CachedRect = finalRect;
        m_IsLayoutDirty = false;

        for (auto& child : m_Children)
        {
            if (child->GetVisibility() != UIVisibility::Collapsed)
            {
                UIRect childRect = child->CalculateBounds(finalRect);
                child->OnArrange(childRect);
            }
        }
    }

    void UIElement::OnDraw(UIBatcher& batcher, const UIRect& screenRect)
    {
        
    }

    void UIElement::OnInspect()
    {
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        strcpy_s(buffer, m_Name.c_str());
        if (ImGui::InputText("Name", buffer, sizeof(buffer)))
        {
            m_Name = std::string(buffer);
        }

        ImGui::Separator();
        ImGui::Text("Transform");
        
        bool isControlledByLayout = false;
        if (auto parent = GetParent())
        {
            if (auto stack = std::dynamic_pointer_cast<StackPanel>(parent))
            {
                isControlledByLayout = true;
                ImGui::TextDisabled("Layout controlled by Parent (StackPanel)");
                ImGui::Separator();
            }
        }

        if (!isControlledByLayout)
        {
            float anchors[4] = { m_Anchor.MinX, m_Anchor.MinY, m_Anchor.MaxX, m_Anchor.MaxY };
            if (ImGui::DragFloat4("Anchors (Min/Max)", anchors, 0.01f, 0.0f, 1.0f))
            {
                m_Anchor = { anchors[0], anchors[1], anchors[2], anchors[3] };
                MarkDirty();
            }
            
            if (ImGui::DragFloat2("Pivot", &m_Pivot.x, 0.01f, 0.0f, 1.0f))
            {
                MarkDirty();
            }

            float offsets[2] = { m_Offset.X, m_Offset.Y };
            if (ImGui::DragFloat2("Pos / Offset", offsets))
            {
                m_Offset = { offsets[0], offsets[1] };
                MarkDirty();
            }

            const char* hAlignItems[] = { "Left", "Center", "Right", "Stretch" };
            int currentHAlign = (int)m_HorizontalAlignment;
            if (ImGui::Combo("Horizontal Alignment", &currentHAlign, hAlignItems, 4))
            {
                SetHorizontalAlignment((UIAlignment)currentHAlign);
            }

            const char* vAlignItems[] = { "Top", "Center", "Bottom", "Stretch" };
            int currentVAlign = (int)m_VerticalAlignment;
            if (ImGui::Combo("Vertical Alignment", &currentVAlign, vAlignItems, 4))
            {
                SetVerticalAlignment((UIAlignment)currentVAlign);
            }
        }

        float size[2] = { m_Size.Width, m_Size.Height };
        if (ImGui::DragFloat2("Size / Delta", size))
        {
            m_Size = { size[0], size[1] };
            MarkDirty();
        }

        ImGui::Separator();
        ImGui::Text("Appearance");

        float opacity = m_Opacity;
        if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 1.0f))
        {
            SetOpacity(opacity);
        }

        const char* visItems[] = { "Visible", "Hidden", "Collapsed" };
        int currentVis = (int)m_Visibility;
        if (ImGui::Combo("Visibility", &currentVis, visItems, 3))
        {
            SetVisibility((UIVisibility)currentVis);
        }
    }

    void UIElement::Serialize(nlohmann::json& outJson) const
    {
        outJson["Name"] = m_Name;
        outJson["Visibility"] = (int)m_Visibility;
        outJson["Opacity"] = m_Opacity;

        outJson["Pivot"] = { m_Pivot.x, m_Pivot.y };
        outJson["Anchor"] = { m_Anchor.MinX, m_Anchor.MinY, m_Anchor.MaxX, m_Anchor.MaxY };
        outJson["Offset"] = { m_Offset.X, m_Offset.Y };
        outJson["Size"] = { m_Size.Width, m_Size.Height };

        // Recursively serialize children
        std::vector<nlohmann::json> childrenArray;
        for (const auto& child : m_Children)
        {
            nlohmann::json childJson;
            // TODO: Polymorphic serialization: we'd save the "Type" string
            // and use a factory to create the correct class on load.
            // For now, we assume everything is UIElement or handled by the parent.
            childJson["Type"] = "UIElement"; // Placeholder
            child->Serialize(childJson);
            childrenArray.push_back(childJson);
        }
        outJson["Children"] = childrenArray;
    }

    void UIElement::Deserialize(const nlohmann::json& json)
    {
        if (json.contains("Name")) m_Name = json["Name"];
        if (json.contains("Visibility")) m_Visibility = (UIVisibility)json["Visibility"];
        if (json.contains("Opacity")) m_Opacity = json["Opacity"];

        if (json.contains("Pivot")) {
            auto& p = json["Pivot"];
            m_Pivot = { p[0], p[1] };
        }
        if (json.contains("Anchor")) {
            auto& a = json["Anchor"];
            m_Anchor = { a[0], a[1], a[2], a[3] };
        }
        if (json.contains("Offset")) {
            auto& o = json["Offset"];
            m_Offset = { o[0], o[1] };
        }
        if (json.contains("Size")) {
            auto& s = json["Size"];
            m_Size = { s[0], s[1] };
        }

        if (json.contains("Children"))
        {
            for (const auto& childJson : json["Children"])
            {
                // TODO: we will check childJson["Type"] to creating "UIImage" or "UIText"
                // For now, we just create base UIElements
                std::shared_ptr<UIElement> child;
                std::string type = childJson.value("Type", "UIElement");
                if (type == "UIImage")
                    child = std::make_shared<UIImage>();
                else if (type == "StackPanel")
                    child = std::make_shared<StackPanel>();
                else
                    child = std::make_shared<UIElement>();

                child->Deserialize(childJson);
                AddChild(child);
            }
        }

        MarkDirty();
    }
    
}
