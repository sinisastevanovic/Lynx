#pragma once
#include "Lynx/UI/Core/UIElement.h"

namespace Lynx
{
    enum class Orientation
    {
        Vertical,
        Horizontal
    };
    
    class LX_API StackPanel : public UIElement
    {
    public:
        StackPanel();
        ~StackPanel() override = default;

        void OnMeasure(UISize availableSize) override;
        void OnArrange(UIRect finalRect) override;

        void SetOrientation(Orientation orientation);
        Orientation GetOrientation() const { return m_Orientation; }

        void SetSpacing(float spacing);
        float GetSpacing() const { return m_Spacing; }

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;

    private:
        Orientation m_Orientation = Orientation::Vertical;
        float m_Spacing = 0.0f;
    };

}
