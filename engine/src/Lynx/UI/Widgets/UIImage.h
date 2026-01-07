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
        Sliced
    };

    class LX_API UIImage : public UIElement
    {
    public:
        UIImage();
        ~UIImage() override = default;

        void OnDraw(UIBatcher& batcher, const UIRect& screenRect) override;

        void SetImage(std::shared_ptr<Asset> image);
        std::shared_ptr<Asset> GetImage() const { return m_ImageResource; }

        void SetColor(const glm::vec4& color) { m_Color = color; }
        glm::vec4 GetColor() const { return m_Color; }

        void SetImageType(ImageType type) { m_ImageType = type; MarkDirty(); }
        ImageType GetImageType() const { return m_ImageType; }

        void SetBorder(UIThickness border) { m_Border = border; MarkDirty(); }
        UIThickness GetBorder() const { return m_Border; }

        void OnInspect() override;
        void Serialize(nlohmann::json& outJson) const override;
        void Deserialize(const nlohmann::json& json) override;

    private:
        void SetTextureInternal(AssetHandle handle);
        void SetMaterialInternal(AssetHandle handle);

    private:
        std::shared_ptr<Asset> m_ImageResource;
        uint32_t m_LastImageVersion = 0;
        glm::vec4 m_Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        ImageType m_ImageType = ImageType::Simple;
        UIThickness m_Border; // For sliced type
    };
}
