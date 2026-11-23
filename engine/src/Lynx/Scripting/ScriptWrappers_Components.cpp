#include "ScriptWrappers.h"

#include "Lynx/Engine.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Scene.h"

#include <stdexcept>

// Gives scripts access to every component that is registered with a field list:
//
//     local stats = player:GetComponent("Character Stats")
//     stats.Strength = stats.Strength + 1
//
// Fields are addressed by their saved key, the first argument of FieldList::Add.

namespace Lynx
{
    namespace
    {
        // A component of one entity, as a script holds it.
        struct ComponentHandle
        {
            Entity Owner;

            // Points into the type registry. Component types stay registered for as long as scripts run.
            const ComponentInfo* Info = nullptr;
        };

        const ComponentInfo* FindComponentInfo(const std::string& name)
        {
            const auto& components = Engine::Get().GetComponentRegistry().GetRegisteredComponents();
            const auto it = components.find(name);
            if (it == components.end())
                throw std::runtime_error("There is no component named '" + name + "'");

            return &it->second;
        }

        void* ResolveComponent(const ComponentHandle& handle)
        {
            if (!handle.Owner)
                throw std::runtime_error("The entity of this '" + handle.Info->name + "' no longer exists");

            void* component = handle.Info->get(handle.Owner.GetScene()->Reg(), handle.Owner);
            if (!component)
                throw std::runtime_error("The entity no longer has a '" + handle.Info->name + "'");

            return component;
        }

        const FieldInfo& FindField(const ComponentHandle& handle, const std::string& key)
        {
            for (const FieldInfo& field : handle.Info->fields)
            {
                if (field.Key == key)
                    return field;
            }

            throw std::runtime_error("'" + handle.Info->name + "' has no field '" + key + "'");
        }

        sol::object ToLua(lua_State* lua, const FieldValue& value)
        {
            return std::visit([lua](const auto& held) { return sol::make_object(lua, held); }, value);
        }

        template<typename T>
        T Expect(const FieldInfo& field, const sol::object& value, const char* expected)
        {
            if (!value.is<T>())
                throw std::runtime_error("Field '" + field.Key + "' expects " + expected);

            return value.as<T>();
        }

        FieldValue FromLua(const FieldInfo& field, const sol::object& value)
        {
            switch (field.Kind)
            {
                case FieldKind::Bool:      return Expect<bool>(field, value, "a boolean");
                case FieldKind::Integer:
                case FieldKind::Enum:      return Expect<int64_t>(field, value, "a whole number");
                case FieldKind::Float:     return Expect<float>(field, value, "a number");
                case FieldKind::Vec2:      return Expect<glm::vec2>(field, value, "a Vec2");
                case FieldKind::Vec3:
                case FieldKind::Color3:    return Expect<glm::vec3>(field, value, "a Vec3");
                case FieldKind::Vec4:
                case FieldKind::Color4:    return Expect<glm::vec4>(field, value, "a Color");
                case FieldKind::Quat:      return Expect<glm::quat>(field, value, "a Quat");
                case FieldKind::String:    return Expect<std::string>(field, value, "a string");
                case FieldKind::UUID:
                case FieldKind::Asset:
                case FieldKind::UIElement:
                    // Scripts hold asset handles and element IDs either as a UUID or as the number behind it.
                    if (value.is<UUID>())
                        return value.as<UUID>();
                    return UUID(Expect<uint64_t>(field, value, "a UUID"));

                case FieldKind::AssetList:
                {
                    // A table of handles, counted from 1 as Lua does.
                    const sol::table entries = Expect<sol::table>(field, value, "a table of UUIDs");
                    std::vector<UUID> handles;
                    for (size_t index = 1; index <= entries.size(); ++index)
                    {
                        const sol::object entry = entries[index];
                        handles.push_back(entry.is<UUID>() ? entry.as<UUID>() : UUID(Expect<uint64_t>(field, entry, "a table of UUIDs")));
                    }
                    return handles;
                }
            }

            throw std::runtime_error("Field '" + field.Key + "' cannot be set from a script");
        }

        sol::optional<ComponentHandle> GetComponent(Entity& entity, const std::string& name)
        {
            const ComponentInfo* info = FindComponentInfo(name);
            if (!info->get)
                throw std::runtime_error("'" + name + "' is not registered with a field list, so scripts cannot read it");

            if (!info->has(entity.GetScene()->Reg(), entity))
                return sol::nullopt;

            return ComponentHandle{ entity, info };
        }
    }

    void ScriptWrappers::RegisterComponentAccess(sol::state& lua)
    {
        lua.new_usertype<ComponentHandle>("Component", sol::no_constructor,
            sol::meta_function::index, [](const ComponentHandle& handle, const std::string& key, sol::this_state state) -> sol::object
            {
                return ToLua(state, FindField(handle, key).Get(ResolveComponent(handle)));
            },
            sol::meta_function::new_index, [](const ComponentHandle& handle, const std::string& key, const sol::object& value)
            {
                const FieldInfo& field = FindField(handle, key);
                field.Set(ResolveComponent(handle), FromLua(field, value));
                handle.Info->notifyChanged(handle.Owner.GetScene()->Reg(), handle.Owner);
            }
        );

        sol::usertype<Entity> entityType = lua["Entity"];

        // Returns nil if the entity does not have the component.
        entityType["GetComponent"] = &GetComponent;

        entityType["HasComponent"] = [](Entity& entity, const std::string& name)
        {
            return FindComponentInfo(name)->has(entity.GetScene()->Reg(), entity);
        };

        // Adds the component with its default values, or returns the one the entity already has.
        entityType["AddComponent"] = [](Entity& entity, const std::string& name)
        {
            const ComponentInfo* info = FindComponentInfo(name);
            if (!info->has(entity.GetScene()->Reg(), entity))
                info->add(entity.GetScene()->Reg(), entity);

            return GetComponent(entity, name);
        };

        entityType["RemoveComponent"] = [](Entity& entity, const std::string& name)
        {
            FindComponentInfo(name)->remove(entity.GetScene()->Reg(), entity);
        };
    }
}
