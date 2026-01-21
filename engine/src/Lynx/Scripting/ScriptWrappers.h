#pragma once
#include <sol/sol.hpp>

#include "Lynx/UI/Widgets/UIButton.h"
#include "Lynx/UI/Widgets/UIImage.h"
#include "Lynx/UI/Widgets/UIText.h"

namespace Lynx
{
    namespace ScriptWrappers
    {
        inline void RegisterBasicTypes(sol::state& lua)
        {
            lua.new_usertype<UUID>("UUID",
                sol::constructors<UUID(), UUID(uint64_t)>(),
                "IsValid", &UUID::IsValid,
                sol::meta_function::to_string, &UUID::ToString,
                sol::meta_function::equal_to, &UUID::operator==
            );
            
            lua["AssetHandle"] = lua["UUID"];
        }
        
        inline void RegisterUITypes(sol::state& lua)
        {
            lua.new_usertype<UIElement>("UIElement",
                "SetVisible", [](UIElement& ui, bool visible) {
                    ui.SetVisibility(visible ? UIVisibility::Visible : UIVisibility::Hidden);
                },
                "GetVisible", [](UIElement& ui) { return ui.GetVisibility() == UIVisibility::Visible; },
                "SetPosition", &UIElement::SetOffset,
                "GetPosition", &UIElement::GetOffset,
                "SetSize", &UIElement::SetSize,
                "GetSize", &UIElement::GetSize,
                "SetName", &UIElement::SetName,
                "GetName", &UIElement::GetName,
                "FindChild", [](UIElement& ui, const std::string& name) -> std::shared_ptr<UIElement> {
                    for(auto& child : ui.GetChildren()) {
                        if(child->GetName() == name) return child;
                    }
                    return nullptr;
                }
            );
            
            lua.new_usertype<UIImage>("UIImage",
                sol::base_classes, sol::bases<UIElement>(),
                "SetColor", &UIImage::SetTint,
                "GetColor", &UIImage::GetTint,
                "SetFillAmount", &UIImage::SetFillAmount,
                "GetFillAmount", &UIImage::GetFillAmount
            );
            
            lua.new_usertype<UIButton>("UIButton",
                sol::base_classes, sol::bases<UIImage, UIElement>(),
                "SetNormalColor", &UIButton::SetNormalColor,
                "SetHoveredColor", &UIButton::SetHoveredColor,
                "SetPressedColor", &UIButton::SetPressedColor,
                // Special OnClick handling with lambda wrapper
                "OnClick", sol::property(
                    [](UIButton& btn) { return sol::nil; },
                    [](UIButton& btn, sol::function callback) {
                        if (callback.valid()) {
                            btn.OnClickEvent = [callback]() {
                                sol::function cb = callback;
                                auto result = cb();
                                if (!result.valid()) {
                                    sol::error err = result;
                                    LX_CORE_ERROR("Lua OnClick Error: {0}", err.what());
                                }
                            };
                        }
                    }
                )
            );
            
            lua.new_usertype<UIText>("UIText",
                sol::base_classes, sol::bases<UIElement>(),
                "SetText", &UIText::SetText,
                "GetText", &UIText::GetText,
                "SetColor", &UIText::SetColor,
                "GetColor", &UIText::GetColor,
                "SetFontSize", &UIText::SetFontSize,
                "GetFontSize", &UIText::GetFontSize
            );
        }
    }
}
