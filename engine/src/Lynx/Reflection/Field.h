#pragma once

#include "Lynx/Core.h"
#include "Lynx/UUID.h"
#include "Lynx/Asset/AssetTypes.h"

#include <glm/glm.hpp>

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

// A component describes its fields once. Saving, loading and the editor's inspector
// are all generated from that description instead of being written by hand per component:
//
//     registry->RegisterComponent<WeaponComponent>("Weapon Component", [](FieldList<WeaponComponent>& fields)
//     {
//         fields.Add("FireRate", &WeaponComponent::FireRate).Label("Fire Rate").DragSpeed(0.1f);
//         fields.Add("ProjectilePrefab", &WeaponComponent::ProjectilePrefab).Label("Bullet Prefab");
//     });
//
// Members that are not listed are runtime state: they are neither saved nor shown.

namespace Lynx
{
    template<typename T> struct AssetRef;
    template<typename T> struct ElementRef;

    enum class FieldKind
    {
        Bool,
        Integer,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Color3,
        Color4,
        String,
        Enum,
        Asset,
        UIElement
    };

    // The value of any field, in the one representation the generic code works with:
    // integers and enums as int64_t, references to assets and UI elements as their UUID.
    using FieldValue = std::variant<bool, int64_t, float, glm::vec2, glm::vec3, glm::vec4, std::string, UUID>;

    struct FieldInfo
    {
        // Name in saved files. Changing it orphans the value in every scene saved before.
        std::string Key;

        // Name shown in the editor.
        std::string Label;

        FieldKind Kind = FieldKind::Bool;

        std::function<FieldValue(const void* component)> Get;
        std::function<void(void* component, const FieldValue& value)> Set;

        // Editor hints. Min equal to Max means the value is not limited.
        bool ShownInEditor = true;
        float DragSpeed = 1.0f;
        float Min = 0.0f;
        float Max = 0.0f;

        // For Enum: the name of each value, starting at 0.
        std::vector<std::string> EnumNames;

        // For Asset: the type of asset the field may refer to.
        AssetType ReferencedAssetType = AssetType::None;
    };

    namespace FieldDetail
    {
        // Converts between a member of type Member and a FieldValue.
        // A member type without a specialization cannot be used as a field.
        template<typename Member>
        struct Adapter;

        template<>
        struct Adapter<bool>
        {
            static constexpr FieldKind Kind = FieldKind::Bool;
            static FieldValue ToValue(bool member) { return member; }
            static void FromValue(bool& member, const FieldValue& value) { member = std::get<bool>(value); }
        };

        template<std::integral Member>
        struct Adapter<Member>
        {
            static constexpr FieldKind Kind = FieldKind::Integer;
            static FieldValue ToValue(Member member) { return static_cast<int64_t>(member); }
            static void FromValue(Member& member, const FieldValue& value) { member = static_cast<Member>(std::get<int64_t>(value)); }
        };

        template<typename Member> requires std::is_enum_v<Member>
        struct Adapter<Member>
        {
            static constexpr FieldKind Kind = FieldKind::Enum;
            static FieldValue ToValue(Member member) { return static_cast<int64_t>(member); }
            static void FromValue(Member& member, const FieldValue& value) { member = static_cast<Member>(std::get<int64_t>(value)); }
        };

        // For the member types a FieldValue holds as they are.
        template<typename Member, FieldKind MemberKind>
        struct DirectAdapter
        {
            static constexpr FieldKind Kind = MemberKind;
            static FieldValue ToValue(const Member& member) { return member; }
            static void FromValue(Member& member, const FieldValue& value) { member = std::get<Member>(value); }
        };

        template<> struct Adapter<float> : DirectAdapter<float, FieldKind::Float> {};
        template<> struct Adapter<glm::vec2> : DirectAdapter<glm::vec2, FieldKind::Vec2> {};
        template<> struct Adapter<glm::vec3> : DirectAdapter<glm::vec3, FieldKind::Vec3> {};
        template<> struct Adapter<glm::vec4> : DirectAdapter<glm::vec4, FieldKind::Vec4> {};
        template<> struct Adapter<std::string> : DirectAdapter<std::string, FieldKind::String> {};

        template<typename Asset>
        struct Adapter<AssetRef<Asset>>
        {
            static constexpr FieldKind Kind = FieldKind::Asset;
            static FieldValue ToValue(const AssetRef<Asset>& member) { return member.Handle; }

            static void FromValue(AssetRef<Asset>& member, const FieldValue& value)
            {
                member.Handle = std::get<UUID>(value);
                member.Cached = nullptr;
            }

            static AssetType ReferencedAssetType()
            {
                if constexpr (requires { Asset::GetStaticType(); })
                    return Asset::GetStaticType();
                else
                    return Asset::GetAssetType();
            }
        };

        template<typename Element>
        struct Adapter<ElementRef<Element>>
        {
            static constexpr FieldKind Kind = FieldKind::UIElement;
            static FieldValue ToValue(const ElementRef<Element>& member) { return member.ID; }

            static void FromValue(ElementRef<Element>& member, const FieldValue& value)
            {
                member.ID = std::get<UUID>(value);
                member.Cached.reset();
            }
        };

        template<typename Member>
        concept SupportedMember = requires { Adapter<Member>::Kind; };
    }

    // Returned by FieldList::Add to set the optional properties of the field that was just added.
    class FieldBuilder
    {
    public:
        FieldBuilder(std::vector<FieldInfo>& fields, size_t index)
            : m_Fields(fields), m_Index(index) {}

        FieldBuilder& Label(std::string label) { Field().Label = std::move(label); return *this; }
        FieldBuilder& DragSpeed(float speed) { Field().DragSpeed = speed; return *this; }
        FieldBuilder& Range(float min, float max) { Field().Min = min; Field().Max = max; return *this; }

        // The field is saved and loaded, but the inspector does not show it.
        FieldBuilder& HideInEditor() { Field().ShownInEditor = false; return *this; }

        // For an enum field: the name of each value, starting at 0.
        FieldBuilder& Names(std::vector<std::string> names) { Field().EnumNames = std::move(names); return *this; }

        // For a vec3 or vec4 field: edit it with a color picker.
        FieldBuilder& AsColor()
        {
            if (Field().Kind == FieldKind::Vec3)
                Field().Kind = FieldKind::Color3;
            else if (Field().Kind == FieldKind::Vec4)
                Field().Kind = FieldKind::Color4;
            return *this;
        }

    private:
        // Looked up by index, because the vector may reallocate when the next field is added.
        FieldInfo& Field() { return m_Fields[m_Index]; }

        std::vector<FieldInfo>& m_Fields;
        size_t m_Index;
    };

    // Collects the fields of the component type T.
    template<typename T>
    class FieldList
    {
    public:
        // The label defaults to the key.
        template<FieldDetail::SupportedMember Member>
        FieldBuilder Add(const std::string& key, Member T::* member)
        {
            using Adapter = FieldDetail::Adapter<Member>;

            FieldInfo field;
            field.Key = key;
            field.Label = key;
            field.Kind = Adapter::Kind;
            field.Get = [member](const void* component) { return Adapter::ToValue(static_cast<const T*>(component)->*member); };
            field.Set = [member](void* component, const FieldValue& value) { Adapter::FromValue(static_cast<T*>(component)->*member, value); };

            if constexpr (requires { Adapter::ReferencedAssetType(); })
                field.ReferencedAssetType = Adapter::ReferencedAssetType();

            m_Fields.push_back(std::move(field));
            return FieldBuilder(m_Fields, m_Fields.size() - 1);
        }

        std::vector<FieldInfo> Take() { return std::move(m_Fields); }

    private:
        std::vector<FieldInfo> m_Fields;
    };
}
