#include "UIImage.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/Asset/Texture.h"
#include "Lynx/ImGui/EditorUIHelpers.h"

namespace Lynx
{
    UIImage::UIImage()
    {
        m_Name = "Image";
    }

    void UIImage::OnDraw(UIBatcher& batcher, const UIRect& screenRect, float scale, glm::vec4 parentTint)
    {
        if (m_Visibility != UIVisibility::Visible)
            return;

        std::shared_ptr<Texture> textureToDraw = nullptr;
        glm::vec2 uvMin = { 0.0f, 0.0f };
        glm::vec2 uvMax = { 1.0f, 1.0f };

        if (m_ImageResource)
        {
            if (m_ImageResource->GetType() == AssetType::Texture)
            {
                auto tex = std::static_pointer_cast<Texture>(m_ImageResource);
                textureToDraw = tex;
            }
            else if (m_ImageResource->GetType() == AssetType::Sprite)
            {
                auto sprite = std::static_pointer_cast<Sprite>(m_ImageResource);
                textureToDraw = sprite->GetTexture();
                uvMin = sprite->GetUVMin();
                uvMax = sprite->GetUVMax();
                if (m_LastImageVersion != m_ImageResource->GetVersion())
                {
                    m_LastImageVersion = m_ImageResource->GetVersion();
                    if (sprite->GetBorder().Left > 0 || sprite->GetBorder().Top > 0 || sprite->GetBorder().Right > 0 || sprite->GetBorder().Bottom > 0)
                    {
                        m_Border = sprite->GetBorder();
                    }
                }
            }
        }

        if (m_ImageType == ImageType::Simple)
        {
            batcher.DrawRect(screenRect, GetTint() * parentTint, m_Material, textureToDraw, uvMin, uvMax);
        }
        else if (m_ImageType == ImageType::Sliced)
        {
            batcher.DrawNineSlice(screenRect, m_Border, GetTint() * parentTint, m_Material, textureToDraw, uvMin, uvMax);
        }
    }

    void UIImage::SetImage(std::shared_ptr<Asset> image)
    {
        if (image->GetType() != AssetType::Texture && image->GetType() != AssetType::Sprite)
        {
            LX_CORE_WARN("UIImage::SetImage called with an asset that is not a Texture or Sprite! Ignoring...");
            return;
        }
        
        m_ImageResource = image;
        m_LastImageVersion = m_ImageResource->GetVersion();

        if (image && image->GetType() == AssetType::Sprite)
        {
            auto sprite = std::static_pointer_cast<Sprite>(image);
            if (sprite->GetBorder().Left > 0 || sprite->GetBorder().Top > 0 || sprite->GetBorder().Right > 0 || sprite->GetBorder().Bottom > 0)
            {
                m_Border = sprite->GetBorder();
                m_ImageType = ImageType::Sliced;
            }
        }
        MarkDirty();
    }

    void UIImage::OnInspect()
    {
        UIElement::OnInspect();
        ImGui::Separator();
        ImGui::Text("Image");
        ImGui::ColorEdit4("Tint", &m_Tint.x);

        AssetHandle texHandle = m_ImageResource ? m_ImageResource->GetHandle() : AssetHandle::Null();
        if (EditorUIHelpers::DrawAssetSelection("Texture", texHandle, { AssetType::Texture, AssetType::Sprite }))
        {
            SetTextureInternal(texHandle);
        }

        AssetHandle matHandle = m_Material ? m_Material->GetHandle() : AssetHandle::Null();
        if (EditorUIHelpers::DrawAssetSelection("Material", matHandle, { AssetType::Material }))
        {
            SetMaterialInternal(matHandle);
        }

        const char* types[] = { "Simple", "Sliced" };
        int currentType = (int)m_ImageType;
        if (ImGui::Combo("Draw As", &currentType, types, 2))
        {
            SetImageType((ImageType)currentType);
        }

        if (m_ImageType == ImageType::Sliced)
        {
            float borders[4] = { m_Border.Left, m_Border.Top, m_Border.Right, m_Border.Bottom };
            if (ImGui::DragFloat4("Borders (L,T,R,B)", borders))
            {
                SetBorder({ borders[0], borders[1], borders[2], borders[3] });
            }
        }
    }

    void UIImage::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);
        outJson["Type"] = "UIImage";
        outJson["Tint"] = m_Tint;
        outJson["Texture"] = (m_ImageResource ? m_ImageResource->GetHandle() : AssetHandle::Null());
        outJson["Material"] = (m_Material ? m_Material->GetHandle() : AssetHandle::Null());
        outJson["ImageType"] = (int)m_ImageType;
        outJson["Border"] = { m_Border.Left, m_Border.Top, m_Border.Right, m_Border.Bottom };
    }

    void UIImage::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);
        if (json.contains("Tint"))
        {
            m_Tint = json["Tint"].get<glm::vec4>();
        }
        if (json.contains("Texture"))
        {
            SetTextureInternal(json["Texture"].get<UUID>());
        }
        if (json.contains("Material"))
        {
            SetMaterialInternal(json["Material"].get<UUID>());
        }
        if (json.contains("ImageType"))
        {
            m_ImageType = (ImageType)json["ImageType"];
        }
        if (json.contains("Border"))
        {
            auto& b = json["Border"];
            m_Border = { b[0], b[1], b[2], b[3] };
        }
    }

    void UIImage::SetTextureInternal(AssetHandle handle)
    {
        // TODO check if image or sprite
        m_ImageResource = handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Asset>(handle) : nullptr;
        m_LastImageVersion = m_ImageResource ? m_ImageResource->GetVersion() : 0;
    }

    void UIImage::SetMaterialInternal(AssetHandle handle)
    {
        m_Material = handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Material>(handle) : nullptr;
    }
}
