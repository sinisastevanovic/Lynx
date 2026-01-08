#pragma once

#include "Lynx/UUID.h"
#include "Lynx/Asset/Asset.h"
#include "Lynx/UI/Core/UIElement.h"

namespace Lynx
{
    class Texture;

    enum class ImageType
    {
        Simple,
        Sliced,
        Filled
    };
    
    enum class FillMethod
    {
        Horizontal,
        Vertical
    };

    class LX_API UIImage : public UIElement
    {
    public:
        UIImage();
        ~UIImage() override = default;

        void OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint) override;

        void SetImage(std::shared_ptr<Asset> image);
        std::shared_ptr<Asset> GetImage() const { return m_ImageResource; }

        void SetTint(const glm::vec4& color) { m_Tint = color; }
        glm::vec4 GetDefaultTint() const { return m_Tint; }
        virtual glm::vec4 GetTint() const { return m_Tint; }

        void SetImageType(ImageType type) { m_ImageType = type; MarkDirty(); }
        ImageType GetImageType() const { return m_ImageType; }

        void SetBorder(UIThickness border) { m_Border = border; MarkDirty(); }
        UIThickness GetBorder() const { return m_Border; }
        
        void SetFillAmount(float amount) { m_FillAmount = amount; }
        float GetFillAmount() const { return m_FillAmount; }
        
        void SetFillMethod(FillMethod method) { m_FillMethod = method; }
        FillMethod GetFillMethod() const { return m_FillMethod; }

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;

    private:
        void SetTextureInternal(AssetHandle handle);
        void SetMaterialInternal(AssetHandle handle);

    protected:
        glm::vec4 m_Tint = { 1.0f, 1.0f, 1.0f, 1.0f };

    private:
        std::shared_ptr<Asset> m_ImageResource;
        uint32_t m_LastImageVersion = 0;
        ImageType m_ImageType = ImageType::Simple;
        UIThickness m_Border; // For sliced type
        FillMethod m_FillMethod = FillMethod::Horizontal; // For filled image type
        float m_FillAmount = 1.0f; // For Filled image type
    };
}
