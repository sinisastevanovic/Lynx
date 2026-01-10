#pragma once

#include "Lynx/Core.h"
#include "UIGeometry.h"
#include <vector>
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace Lynx
{
    class Material;
    class UIBatcher;

    enum class UIVisibility
    {
        Visible,
        Hidden,
        Collapsed
    };

    class LX_API UIElement : public std::enable_shared_from_this<UIElement>
    {
    public:
        UIElement();
        virtual ~UIElement() = default;

        // Hierarchy
        void AddChild(std::shared_ptr<UIElement> child);
        void AddChildAt(std::shared_ptr<UIElement> child, size_t index);
        void RemoveChild(std::shared_ptr<UIElement> child);
        std::shared_ptr<UIElement> GetParent() const { return m_Parent.lock(); }
        const std::vector<std::shared_ptr<UIElement>>& GetChildren() const { return m_Children; }
        void MoveChild(std::shared_ptr<UIElement> child, size_t newIndex);
        void ClearChildren();
        bool IsDescendantOf(std::shared_ptr<UIElement> other) const;
        
        UUID GetUUID() const { return m_UUID; }

        // Layout Props
        void SetOffset(UIPoint offset);
        UIPoint GetOffset() const { return m_Offset; }
        void SetSize(UISize size);
        UISize GetSize() const { return m_Size; }
        UISize GetDesiredSize() const { return m_DesiredSize; }
        void SetAnchor(UIAnchor anchor);
        UIAnchor GetAnchor() const { return m_Anchor; }
        void SetPivot(glm::vec2 pivot);
        glm::vec2 GetPivot() const { return m_Pivot; }
        void SetHorizontalAlignment(UIAlignment alignment);
        UIAlignment GetHorizontalAlignment() const { return m_HorizontalAlignment; }
        void SetVerticalAlignment(UIAlignment alignment);
        UIAlignment GetVerticalAlignment() const { return m_VerticalAlignment; }
        void SetPadding(UIThickness padding);
        UIThickness GetPadding() const { return m_Padding; }

        // Visual Props
        void SetVisibility(UIVisibility visibility);
        UIVisibility GetVisibility() const { return m_Visibility; }
        void SetOpacity(float opacity) { m_Opacity = glm::clamp(opacity, 0.0f, 1.0f); }
        float GetOpacity() const { return m_Opacity; }
        void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }
        std::shared_ptr<Material> GetMaterial() const { return m_Material; }
        glm::vec4 GetContentColor() const { return m_ContentColor; }
        void SetClipChildren(bool clipChildren) { m_ClipChildren = clipChildren; }
        bool GetClipChildren() const { return m_ClipChildren; }

        // Logic
        virtual void OnUpdate(float deltaTime);
        virtual void OnMeasure(UISize availableSize);
        virtual void OnArrange(UIRect finalRect);
        virtual void OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint);

        // Interaction
        virtual bool HitTest(const glm::vec2& point, bool ignoreHitTestVisible = false);
        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_IsEnabled; }
        virtual void OnMouseEnter() {}
        virtual void OnMouseLeave() {}
        virtual void OnMouseDown() {}
        virtual void OnMouseUp() {}
        virtual void OnClick() {}
        virtual bool OnMouseScroll(float offsetX, float offsetY) { return false; }

        bool IsMouseOver() const { return m_IsMouseOver; }
        bool IsPressed() const { return m_IsPressed; }
        void SetHitTestVisible(bool visible) { m_IsHitTestVisible = visible; }
        bool GetHitTestVisible() const { return m_IsHitTestVisible; }

        // Editor & Serialization
        virtual void OnInspect();
        virtual void Serialize(nlohmann::json& outJson) const;
        virtual void Deserialize(const nlohmann::json& json);
        
        // Metadata
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        UIRect GetCachedRect() { return m_CachedRect; }
        
        static std::shared_ptr<UIElement> CreateFromType(std::string type);

    protected:
        void MarkDirty();
        UIRect CalculateBounds(UIRect parentRect) const;

    protected:
        std::string m_Name = "UIElement";
        std::weak_ptr<UIElement> m_Parent;
        std::vector<std::shared_ptr<UIElement>> m_Children;
        
        UUID m_UUID;

        // Transformation Data
        UIAnchor m_Anchor;                                  // 0-1 range relative to parent
        glm::vec2 m_Pivot = {0.5f, 0.5f };              // 0-1 range relative to self
        UIPoint m_Offset = {0.0f, 0.0f };             // Positional offset in DP
        UISize m_Size = { 100.0f, 100.0f };    // Size in DP
        UISize m_DesiredSize = { 0, 0 };
        UIAlignment m_HorizontalAlignment = UIAlignment::Stretch;
        UIAlignment m_VerticalAlignment = UIAlignment::Stretch;
        UIThickness m_Padding = { 0.0f, 0.0f, 0.0f, 0.0f };

        UIVisibility m_Visibility = UIVisibility::Visible;
        float m_Opacity = 1.0f;
        glm::vec4 m_ContentColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        UIRect m_CachedRect;    // Calculated in Arrange phase
        std::shared_ptr<Material> m_Material; // TODO: Does it make sense to have this here? This is just a base class with no rendering...

        bool m_IsMouseOver = false;
        bool m_IsPressed = false;
        bool m_IsHitTestVisible = false;
        bool m_IsEnabled = true;
        bool m_ClipChildren = false;

        bool m_IsLayoutDirty = true;

        friend class UICanvas;
        friend class UIScrollView;
    };
}
