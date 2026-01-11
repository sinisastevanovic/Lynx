#include "UIScrollView.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/ImGui/LXUI.h"

namespace Lynx
{
    UIScrollView::UIScrollView()
    {
        m_Name = "UIScrollView";
        m_ClipChildren = true;
        m_IsHitTestVisible = true;
    }

    void UIScrollView::SetScrollPosition(const glm::vec2& pos)
    {
        float maxScroll = std::max(0.0f, m_ContentSize.Height - m_CachedRect.Height);
        m_ScrollPosition = pos;
        m_ScrollPosition.y = glm::clamp(m_ScrollPosition.y, 0.0f, maxScroll);
        
        MarkDirty();
    }

    void UIScrollView::SetVerticalScrollbar(std::shared_ptr<UIElement> verticalScrollbar)
    {
        if (verticalScrollbar)
        {
            if (!verticalScrollbar->IsDescendantOf(shared_from_this()))
            {
                LX_CORE_ERROR("Scrollbar needs to be a child of this ScrollView!");
                return;
            }
            verticalScrollbar->SetAnchor(UIAnchor::TopRight);
        }
        m_VerticalScrollbar = verticalScrollbar;
        UpdateScrollBarState();
        MarkDirty();
    }

    bool UIScrollView::OnMouseScroll(float offsetX, float offsetY)
    {
        float delta = offsetY * m_ScrollSensitivity;
        m_ScrollPosition.y -= delta;
        
        float maxScroll = std::max(0.0f, m_ContentSize.Height - m_CachedRect.Height);
        m_ScrollPosition.y = glm::clamp(m_ScrollPosition.y, 0.0f, maxScroll);
        
        MarkDirty();
        return true;
    }

    void UIScrollView::OnMeasure(UISize availableSize)
    {
        UIElement::OnMeasure(availableSize);
        
        UISize contentAvailable = availableSize;
        contentAvailable.Height = 100000.0f;
        
        m_ContentSize = { 0, 0 };
        for (auto& child : m_Children)
        {
            if (child->GetVisibility() == UIVisibility::Collapsed || child == m_VerticalScrollbar)
                continue;
            
            child->OnMeasure(contentAvailable);
            
            UISize explicitSize = child->GetSize();
            UISize desiredSize = child->GetDesiredSize();
            UISize effectiveSize = {
                glm::abs(explicitSize.Width > 0.001f) ? explicitSize.Width : desiredSize.Width,
                glm::abs(explicitSize.Height > 0.001f) ? explicitSize.Height : desiredSize.Height,
            };
            
            m_ContentSize.Width = std::max(m_ContentSize.Width, effectiveSize.Width);
            m_ContentSize.Height = std::max(m_ContentSize.Height, effectiveSize.Height);
        }
    }

    void UIScrollView::OnArrange(UIRect finalRect)
    {
        m_CachedRect = finalRect;
        m_IsLayoutDirty = false;

        float scrollbarWidth = 0.0f;
        float contentH = m_ContentSize.Height;
        if (contentH > finalRect.Height)
        {
            if (m_VerticalScrollbar)
            {
                scrollbarWidth = m_VerticalScrollbar->GetSize().Width;
                if (scrollbarWidth < 1.0f)
                    scrollbarWidth = 10.0f;
            }
        }
        
        UIThickness effectivePadding = m_Padding;
        effectivePadding.Right += scrollbarWidth;

        UIRect contentRect = finalRect;
        contentRect.X += effectivePadding.Left;
        contentRect.Y += effectivePadding.Top;
        contentRect.Width -= (effectivePadding.Left + effectivePadding.Right);
        contentRect.Height -= (effectivePadding.Top + effectivePadding.Bottom);
        contentRect.Width = std::max(0.0f, contentRect.Width);
        contentRect.Height = std::max(0.0f, contentRect.Height);
        contentRect.X -= m_ScrollPosition.x;
        contentRect.Y -= m_ScrollPosition.y;
        // contentRect.Width / Height should be large enough?
        // Actually, CalculateBounds for the child should handle it if child is anchored Top-Left.
        
        UpdateScrollBarState();

        for (auto& child : m_Children)
        {
            if (child->GetVisibility() != UIVisibility::Collapsed)
            {
                if (child == m_VerticalScrollbar)
                {
                    UIRect barRect = child->CalculateBounds(finalRect);
                    child->OnArrange(barRect);
                }
                else
                {
                    // Calculate child bounds relative to the SHIFTED viewport
                    UIRect childRect = child->CalculateBounds(contentRect);
                    child->OnArrange(childRect);
                }
            }
        }
    }
    
    void UIScrollView::OnPostLoad()
    {
        UIElement::OnPostLoad();
        if (m_VScrollbarID.IsValid())
        {
            if (auto root = GetRoot())
            {
                auto bar = root->FindElementByID(m_VScrollbarID);
                SetVerticalScrollbar(bar);
            }
        }
    }

    void UIScrollView::OnInspect()
    {
        UIElement::OnInspect();
        
        if (LXUI::PropertyGroup("Scroll Viewer"))
        {
            LXUI::DrawDragFloat("ScrollSensitivity", m_ScrollSensitivity, 0.5f, 0.01f, FLT_MAX, 20.0f);
            
            UUID scrollbarID = m_VerticalScrollbar ? m_VerticalScrollbar->GetUUID() : UUID::Null();
            Scene* scene = Engine::Get().GetActiveScene().get();
            if (LXUI::DrawUIElementSelection("Vertical Scrollbar", scrollbarID, scene))
            {
                if (scrollbarID.IsValid())
                {
                    SetVerticalScrollbar(scene->FindUIElementByID(scrollbarID));
                }
                else
                {
                    SetVerticalScrollbar(nullptr);
                }
            }
        }
    }

    void UIScrollView::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);
        outJson["Type"] = "UIScrollView";
        outJson["ScrollPosition"] = m_ScrollPosition;
        outJson["VerticalScrollbar"] = m_VerticalScrollbar ? m_VerticalScrollbar->GetUUID() : UUID::Null();
    }

    void UIScrollView::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);
        m_ScrollPosition = json["ScrollPosition"];
        m_VScrollbarID = json["VerticalScrollbar"].get<UUID>();
    }

    void UIScrollView::UpdateScrollBarState()
    {
        if (!m_VerticalScrollbar)
            return;
        
        float viewportH = m_CachedRect.Height;
        float contentH = m_ContentSize.Height;
        
        if (contentH <= viewportH || contentH < 0.001f)
        {
            m_VerticalScrollbar->SetVisibility(UIVisibility::Hidden);
            return;
        }
        
        m_VerticalScrollbar->SetVisibility(UIVisibility::Visible);
        
        float viewRatio = viewportH / contentH;
        float maxScroll = contentH - viewportH;
        float scrollRatio = m_ScrollPosition.y / maxScroll;
        
        float handleSize = glm::clamp(viewRatio, 0.1f, 1.0f);
        float travelSpace = 1.0f - handleSize;
        float handleStart = scrollRatio * travelSpace;
        float handleEnd = handleStart + handleSize;
        
        UIAnchor current = m_VerticalScrollbar->GetAnchor();
        current.MinY = handleStart;
        current.MaxY = handleEnd;
        m_VerticalScrollbar->SetAnchor(current);
        
        m_VerticalScrollbar->SetOffset({0, 0});
        UISize currSize = m_VerticalScrollbar->GetSize();
        m_VerticalScrollbar->SetSize({currSize.Width, 0.0f});
    }
}
