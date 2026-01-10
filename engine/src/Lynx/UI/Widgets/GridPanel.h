#pragma once
#include "Lynx/UI/Core/UIElement.h"

namespace Lynx
{
    class LX_API GridPanel : public UIElement
    {
    public:
        GridPanel();
        ~GridPanel() override = default;
        
        void SetCellSize(const glm::vec2& size);
        glm::vec2 GetCellSize() const { return m_CellSize; }
        void SetSpacing(const glm::vec2& spacing);
        glm::vec2 GetSpacing() const { return m_Spacing; }
        void SetConstraint(int columns);
        
        void OnMeasure(UISize availableSize) override;
        void OnArrange(UIRect finalRect) override;
        
        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;
        
    private:
        glm::vec2 m_CellSize = { 100.0f, 100.0f };
        glm::vec2 m_Spacing = { 10.0f, 10.0f };
        int m_ConstraintCount = 3;
    };

}
