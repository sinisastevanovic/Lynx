#include "UIElement.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "Lynx/ImGui/LXUI.h"
#include "Lynx/UI/Rendering/UIBatcher.h"
#include "Lynx/UI/Widgets/GridPanel.h"
#include "Lynx/UI/Widgets/StackPanel.h"
#include "Lynx/UI/Widgets/UIButton.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIScrollView.h"
#include "Lynx/UI/Widgets/UIText.h"

namespace Lynx
{
    UIElement::UIElement()
    {
        m_Anchor = {0.5f, 0.5f, 0.5f, 0.5f};
        m_Pivot = {0.5f, 0.5f};
    }

    void UIElement::AddChild(std::shared_ptr<UIElement> child)
    {
        if (child->GetParent())
            child->GetParent()->RemoveChild(child);

        child->m_Parent = shared_from_this();
        m_Children.push_back(child);
        MarkDirty();
    }

    void UIElement::AddChildAt(std::shared_ptr<UIElement> child, size_t index)
    {
        if (child->GetParent())
            child->GetParent()->RemoveChild(child);

        child->m_Parent = shared_from_this();
        if (index >= m_Children.size())
            m_Children.push_back(child);
        else
            m_Children.insert(m_Children.begin() + index, child);
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

    void UIElement::MoveChild(std::shared_ptr<UIElement> child, size_t newIndex)
    {
        auto& children = m_Children;
        auto it = std::find(children.begin(), children.end(), child);
        if (it == children.end()) return;

        size_t oldIndex = std::distance(children.begin(), it);

        if (oldIndex < newIndex)
            newIndex--;

        children.erase(it);

        if (newIndex >= children.size())
            children.push_back(child);
        else
            children.insert(children.begin() + newIndex, child);

        MarkDirty();
    }

    void UIElement::ClearChildren()
    {
        for (auto& child : m_Children)
            child->m_Parent.reset();
        m_Children.clear();
        MarkDirty();
    }

    bool UIElement::IsDescendantOf(std::shared_ptr<UIElement> other) const
    {
        std::shared_ptr<UIElement> parent = GetParent();
        while (parent)
        {
            if (parent == other)
                return true;
            parent = parent->GetParent();
        }
        return false;
    }

    std::shared_ptr<UIElement> UIElement::GetRoot() const
    {
        auto root = GetParent();
        while (root && root->GetParent())
            root = root->GetParent();
        return root;
    }

    std::shared_ptr<UIElement> UIElement::FindElementByID(UUID id)
    {
        if (m_UUID == id)
            return shared_from_this();
        for (auto& child : m_Children)
        {
            auto found = child->FindElementByID(id);
            if (found)
                return found;
        }
        return nullptr;
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

    void UIElement::SetPadding(UIThickness padding)
    {
        m_Padding = padding;
        MarkDirty();
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
        UISize childAvailable = availableSize;
        
        if (m_Size.Width > 0.0f) childAvailable.Width = m_Size.Width;
        if (m_Size.Height > 0.0f) childAvailable.Height = m_Size.Height;
        
        childAvailable.Width -= (m_Padding.Left + m_Padding.Right);
        childAvailable.Height -= (m_Padding.Top + m_Padding.Bottom);
        childAvailable.Width = std::max(0.0f, childAvailable.Width);
        childAvailable.Height = std::max(0.0f, childAvailable.Height);
        
        float maxWidth = 0.0f;
        float maxHeight = 0.0f;

        for (auto& child : m_Children)
        {
            if (child->GetVisibility() == UIVisibility::Collapsed)
                continue;

            child->OnMeasure(childAvailable);

            UISize explicitSize = child->GetSize();
            UISize desiredSize = child->GetDesiredSize();
            UISize effectiveSize = {
                glm::abs(explicitSize.Width > 0.001f) ? explicitSize.Width : desiredSize.Width,
                glm::abs(explicitSize.Height > 0.001f) ? explicitSize.Height : desiredSize.Height,
            };

            float right = child->GetOffset().X + effectiveSize.Width;
            float bottom = child->GetOffset().Y + effectiveSize.Height;

            if (right > maxWidth) maxWidth = right;
            if (bottom > maxHeight) maxHeight = bottom;
        }

        m_DesiredSize.Width = maxWidth + m_Padding.Left + m_Padding.Right;
        m_DesiredSize.Height = maxHeight + m_Padding.Top + m_Padding.Bottom;
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

        UIRect contentRect = finalRect;
        contentRect.X += m_Padding.Left;
        contentRect.Y += m_Padding.Top;
        contentRect.Width -= (m_Padding.Left + m_Padding.Right);
        contentRect.Height -= (m_Padding.Top + m_Padding.Bottom);
        contentRect.Width = std::max(0.0f, contentRect.Width);
        contentRect.Height = std::max(0.0f, contentRect.Height);

        for (auto& child : m_Children)
        {
            if (child->GetVisibility() != UIVisibility::Collapsed)
            {
                UIRect childRect = child->CalculateBounds(contentRect);
                child->OnArrange(childRect);
            }
        }
    }

    void UIElement::OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint)
    {
    }

    void UIElement::OnPostLoad()
    {
        for (auto& child : m_Children)
            child->OnPostLoad();
    }

    bool UIElement::HitTest(const glm::vec2& point, bool ignoreHitTestVisible)
    {
        if (m_Visibility != UIVisibility::Visible || (!m_IsHitTestVisible && !ignoreHitTestVisible))
            return false;

        return (point.x >= m_CachedRect.X && point.x <= m_CachedRect.X + m_CachedRect.Width &&
            point.y >= m_CachedRect.Y && point.y <= m_CachedRect.Y + m_CachedRect.Height);
    }

    void UIElement::SetEnabled(bool enabled)
    {
        m_IsEnabled = enabled;
        m_ContentColor = m_IsEnabled ? glm::vec4(1.0f) : glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    }

    void UIElement::OnInspect()
    {
        LXUI::DrawTextInput("Name", m_Name);

        if (LXUI::PropertyGroup("Transform"))
        {
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
                UIAnchor currAnchor = m_Anchor;
                if (LXUI::DrawUIAnchorControl("Anchors (Min/Max)", currAnchor)) // TODO: Use special UI Labels for anchors and other stuff...
                {
                    m_Anchor = currAnchor;
                    MarkDirty();
                }

                if (LXUI::DrawVec2Control("Pivot", m_Pivot, 0.01f, 0, 1, 0.5f))
                {
                    MarkDirty();
                }

                glm::vec2 offsets = {m_Offset.X, m_Offset.Y};
                if (LXUI::DrawVec2Control("Pos / Offset", offsets, 1.0f))
                {
                    m_Offset = {offsets.x, offsets.y};
                    MarkDirty();
                }

                std::vector<std::string> hAlignItems = {"Left", "Center", "Right", "Stretch"};
                int currentHAlign = (int)m_HorizontalAlignment;
                if (LXUI::DrawComboControl("Horizontal Alignment", currentHAlign, hAlignItems))
                {
                    SetHorizontalAlignment((UIAlignment)currentHAlign);
                }

                std::vector<std::string> vAlignItems = {"Top", "Center", "Bottom", "Stretch"};
                int currentVAlign = (int)m_VerticalAlignment;
                if (LXUI::DrawComboControl("Vertical Alignment", currentVAlign, vAlignItems))
                {
                    SetVerticalAlignment((UIAlignment)currentVAlign);
                }
            }

            glm::vec2 size = {m_Size.Width, m_Size.Height};
            if (LXUI::DrawVec2Control("Size / Delta", size, 1.0f, 0, 0, 0))
            {
                m_Size = {size.x, size.y};
                MarkDirty();
            }
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            if (ImGui::Button("Auto Size"))
            {
                m_Size = { 0, 0 };
                MarkDirty();
            }

            UIThickness thickness = m_Padding;
            if (LXUI::DrawUIThicknessControl("Padding", thickness))
            {
                m_Padding = thickness;
                MarkDirty();
            }
        }
        
        if (LXUI::PropertyGroup("Appearance"))
        {
            float opacity = m_Opacity;
            if (LXUI::DrawSliderFloat("Opacity", opacity, 0.0f, 1.0f))
            {
                SetOpacity(opacity);
            }

            std::vector<std::string> visItems = {"Visible", "Hidden", "Collapsed"};
            int currentVis = (int)m_Visibility;
            if (LXUI::DrawComboControl("Visibility", currentVis, visItems))
            {
                SetVisibility((UIVisibility)currentVis);
            }
        }
        
        if (LXUI::PropertyGroup("Misc"))
        {
            LXUI::DrawCheckBox("Hit Test Visible", m_IsHitTestVisible);
            bool enabled = m_IsEnabled;
            if (LXUI::DrawCheckBox("Enabled", enabled))
            {
                SetEnabled(enabled);
            }
            LXUI::DrawCheckBox("Clip Children", m_ClipChildren);
        }
    }

    void UIElement::Serialize(nlohmann::json& outJson) const
    {
        outJson["UUID"] = m_UUID;
        outJson["Name"] = m_Name;
        outJson["Visibility"] = (int)m_Visibility;
        outJson["Opacity"] = m_Opacity;

        outJson["Pivot"] = {m_Pivot.x, m_Pivot.y};
        outJson["Anchor"] = {m_Anchor.MinX, m_Anchor.MinY, m_Anchor.MaxX, m_Anchor.MaxY};
        outJson["Offset"] = {m_Offset.X, m_Offset.Y};
        outJson["Size"] = {m_Size.Width, m_Size.Height};
        outJson["HorizontalAlignment"] = (int)m_HorizontalAlignment;
        outJson["VerticalAlignment"] = (int)m_VerticalAlignment;
        outJson["Padding"] = {m_Padding.Left, m_Padding.Top, m_Padding.Right, m_Padding.Bottom};
        outJson["HitTestVisible"] = m_IsHitTestVisible;
        outJson["Enabled"] = m_IsEnabled;
        outJson["ClipChildren"] = m_ClipChildren;

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
        if (json.contains("UUID")) m_UUID = json["UUID"].get<UUID>();
        if (json.contains("Name")) m_Name = json["Name"];
        if (json.contains("Visibility")) m_Visibility = (UIVisibility)json["Visibility"];
        if (json.contains("Opacity")) m_Opacity = json["Opacity"];

        if (json.contains("Pivot"))
        {
            auto& p = json["Pivot"];
            m_Pivot = {p[0], p[1]};
        }
        if (json.contains("Anchor"))
        {
            auto& a = json["Anchor"];
            m_Anchor = {a[0], a[1], a[2], a[3]};
        }
        if (json.contains("Offset"))
        {
            auto& o = json["Offset"];
            m_Offset = {o[0], o[1]};
        }
        if (json.contains("Size"))
        {
            auto& s = json["Size"];
            m_Size = {s[0], s[1]};
        }
        if (json.contains("HorizontalAlignment"))
            m_HorizontalAlignment = (UIAlignment)json["HorizontalAlignment"];
        if (json.contains("VerticalAlignment"))
            m_VerticalAlignment = (UIAlignment)json["VerticalAlignment"];
        if (json.contains("Padding"))
        {
            auto& p = json["Padding"];
            m_Padding = {p[0], p[1], p[2], p[3]};
        }
        if (json.contains("HitTestVisible"))
            m_IsHitTestVisible = (bool)json["HitTestVisible"];
        if (json.contains("Enabled"))
            m_IsEnabled = (bool)json["Enabled"];
        if (json.contains("ClipChildren"))
            m_ClipChildren = (bool)json["ClipChildren"];

        if (json.contains("Children"))
        {
            for (const auto& childJson : json["Children"])
            {
                // TODO: we will check childJson["Type"] to creating "UIImage" or "UIText"
                // For now, we just create base UIElements
                std::string type = childJson.value("Type", "UIElement");
                std::shared_ptr<UIElement> child = CreateFromType(type);
                child->Deserialize(childJson);
                AddChild(child);
            }
        }

        MarkDirty();
    }

    std::shared_ptr<UIElement> UIElement::CreateFromType(std::string type)
    {
        if (type == "UIImage")
            return std::make_shared<UIImage>();
        else if (type == "StackPanel")
            return std::make_shared<StackPanel>();
        else if (type == "GridPanel")
            return std::make_shared<GridPanel>();
        else if (type == "UIText")
            return std::make_shared<UIText>();
        else if (type == "UIButton")
            return std::make_shared<UIButton>();
        else if (type == "UIScrollView")
            return std::make_shared<UIScrollView>();
        else
            return std::make_shared<UIElement>();
    }
}
