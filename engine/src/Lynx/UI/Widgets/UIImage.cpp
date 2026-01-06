#include "UIImage.h"

#include <imgui.h>

#include "Lynx/Engine.h"
#include "Lynx/Asset/Texture.h"
#include "Lynx/ImGui/EditorUIHelpers.h"

namespace Lynx
{
    UIImage::UIImage()
    {
        m_Name = "Image";
    }

    void UIImage::OnDraw(UIBatcher& batcher, const UIRect& screenRect)
    {
        if (m_Visibility != UIVisibility::Visible)
            return;

        batcher.DrawRect(screenRect, m_Color, m_Material, m_Texture);
    }

    void UIImage::SetTexture(std::shared_ptr<Texture> texture)
    {
        if (m_Texture != texture)
        {
            m_Texture = texture;
            MarkDirty();
        }
    }

    void UIImage::OnInspect()
    {
        UIElement::OnInspect();
        ImGui::Separator();
        ImGui::Text("Image");
        ImGui::ColorEdit4("Tint", &m_Color.x);

        AssetHandle texHandle = m_Texture ? m_Texture->GetHandle() : AssetHandle::Null();
        if (EditorUIHelpers::DrawAssetSelection("Texture", texHandle, AssetType::Texture))
        {
            SetTextureInternal(texHandle);
        }

        AssetHandle matHandle = m_Material ? m_Material->GetHandle() : AssetHandle::Null();
        if (EditorUIHelpers::DrawAssetSelection("Material", matHandle, AssetType::Material))
        {
            SetMaterialInternal(matHandle);
        }
    }

    void UIImage::Serialize(nlohmann::json& outJson) const
    {
        UIElement::Serialize(outJson);
        outJson["Type"] = "UIImage";
        outJson["Color"] = m_Color;
        outJson["Texture"] = (m_Texture ? m_Texture->GetHandle() : AssetHandle::Null());
        outJson["Material"] = (m_Material ? m_Material->GetHandle() : AssetHandle::Null());
    }

    void UIImage::Deserialize(const nlohmann::json& json)
    {
        UIElement::Deserialize(json);
        if (json.contains("Color"))
        {
            m_Color = json["Color"].get<glm::vec4>();
        }
        if (json.contains("Texture"))
        {
            SetTextureInternal(json["Texture"].get<UUID>());
        }
        if (json.contains("Material"))
        {
            SetMaterialInternal(json["Material"].get<UUID>());
        }
    }

    void UIImage::SetTextureInternal(AssetHandle handle)
    {
        m_Texture = handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Texture>(handle) : nullptr;
    }

    void UIImage::SetMaterialInternal(AssetHandle handle)
    {
        m_Material = handle.IsValid() ? Engine::Get().GetAssetManager().GetAsset<Material>(handle) : nullptr;
    }
}
