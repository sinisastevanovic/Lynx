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
        void RemoveChild(std::shared_ptr<UIElement> child);
        std::shared_ptr<UIElement> GetParent() const { return m_Parent.lock(); }
        const std::vector<std::shared_ptr<UIElement>>& GetChildren() const { return m_Children; }
        void ClearChildren();

        // Layout Props
        void SetOffset(UIPoint offset);
        UIPoint GetOffset() const { return m_Offset; }
        void SetSize(UISize size);
        UISize GetSize() const { return m_Size; }
        void SetAnchor(UIAnchor anchor);
        UIAnchor GetAnchor() const { return m_Anchor; }
        void SetPivot(glm::vec2 pivot);
        glm::vec2 GetPivot() const { return m_Pivot; }

        // Visual Props
        void SetVisibility(UIVisibility visibility);
        UIVisibility GetVisibility() const { return m_Visibility; }
        void SetOpacity(float opacity) { m_Opacity = glm::clamp(opacity, 0.0f, 1.0f); }
        float GetOpacity() const { return m_Opacity; }
        void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }
        std::shared_ptr<Material> GetMaterial() const { return m_Material; }

        // Logic
        virtual void OnUpdate(float deltaTime);
        virtual void OnMeasure(UISize availableSize);
        virtual void OnArrange(UIRect finalRect);
        virtual void OnDraw(UIBatcher& batcher, const UIRect& screenRect) {}

        // Editor & Serialization
        virtual void OnInspect();
        virtual void Serialize(nlohmann::json& outJson) const;
        virtual void Deserialize(const nlohmann::json& json);
        
        // Metadata
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        UIRect GetCachedRect() { return m_CachedRect; }

    protected:
        void MarkDirty();
        UIRect CalculateBounds(UIRect parentRect) const;

    protected:
        std::string m_Name = "UIElement";
        std::weak_ptr<UIElement> m_Parent;
        std::vector<std::shared_ptr<UIElement>> m_Children;

        // Transformation Data
        UIAnchor m_Anchor;                                  // 0-1 range relative to parent
        glm::vec2 m_Pivot = {0.5f, 0.5f };              // 0-1 range relative to self
        UIPoint m_Offset = {0.0f, 0.0f };             // Positional offset in DP
        UISize m_Size = { 100.0f, 100.0f };    // Size in DP

        UIVisibility m_Visibility = UIVisibility::Visible;
        float m_Opacity = 1.0f;

        UIRect m_CachedRect;    // Calculated in Arrange phase
        std::shared_ptr<Material> m_Material; // TODO: Does it make sense to have this here? This is just a base class with no rendering...

        bool m_IsLayoutDirty = true;
    };
}
