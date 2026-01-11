#include "UIImage.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Sprite.h"
#include "Lynx/Asset/Texture.h"
#include "Lynx/ImGui/LXUI.h"

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
        
        glm::vec4 finalColor = GetTint() * parentTint;

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
            batcher.DrawRect(screenRect, finalColor, m_Material, textureToDraw, uvMin, uvMax);
        }
        else if (m_ImageType == ImageType::Sliced)
        {
            batcher.DrawNineSlice(screenRect, m_Border, finalColor, m_Material, textureToDraw, uvMin, uvMax);
        }
        else if (m_ImageType == ImageType::Filled)
        {
            UIRect filledRect = screenRect;
            glm::vec2 filledUVMin = uvMin;
            glm::vec2 filledUVMax = uvMax;
            
            float fill = glm::clamp(m_FillAmount, 0.0f, 1.0f);
            
            if (m_FillMethod == FillMethod::Horizontal)
            {
                filledRect.Width *= fill;
                filledUVMax.x = glm::mix(uvMin.x, uvMax.x, fill);
            }
            else if (m_FillMethod == FillMethod::Vertical)
            {
                float visibleHeight = filledRect.Height * fill;
                float hiddenHeight = filledRect.Height * (1.0f - fill);

                filledRect.Y += hiddenHeight;
                filledRect.Height = visibleHeight;
                
                filledUVMin.y = glm::mix(uvMax.y, uvMin.y, fill);
            }
            
            batcher.DrawRect(filledRect, finalColor, m_Material, textureToDraw, filledUVMin, filledUVMax);
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
        
        if (LXUI::PropertyGroup("Image"))
        {
            LXUI::DrawColorControl("Tint", m_Tint);

            AssetHandle texHandle = m_ImageResource ? m_ImageResource->GetHandle() : AssetHandle::Null();
            if (LXUI::DrawAssetReference("Texture", texHandle, { AssetType::Texture, AssetType::Sprite }))
            {
                SetTextureInternal(texHandle);
            }

            AssetHandle matHandle = m_Material ? m_Material->GetHandle() : AssetHandle::Null();
            if (LXUI::DrawAssetReference("Material", matHandle, { AssetType::Material }))
            {
                SetMaterialInternal(matHandle);
            }

            std::vector<std::string> types = { "Simple", "Sliced", "Filled" };
            int currentType = (int)m_ImageType;
            if (LXUI::DrawComboControl("Draw As", currentType, types))
            {
                SetImageType((ImageType)currentType);
            }

            if (m_ImageType == ImageType::Sliced)
            {
                UIThickness borders = m_Border;
                if (LXUI::DrawUIThicknessControl("Borders", borders))
                {
                    SetBorder(borders);
                }
            }
            else if (m_ImageType == ImageType::Filled)
            {
                LXUI::DrawSliderFloat("Fill Amount", m_FillAmount, 0.0f, 1.0f);
                std::vector<std::string> fillTypes = { "Horizontal", "Vertical" };
                int currentFillType = (int)m_FillMethod;
                if (LXUI::DrawComboControl("Fill Method", currentFillType, fillTypes))
                {
                    SetFillMethod((FillMethod)currentFillType);
                }
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
        outJson["FillAmount"] = m_FillAmount;
        outJson["FillMethod"] = (int)m_FillMethod;
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
        if (json.contains("FillAmount"))
        {
            m_FillAmount = json["FillAmount"];
        }
        if (json.contains("FillMethod"))
        {
            m_FillMethod = (FillMethod)json["FillMethod"];
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
