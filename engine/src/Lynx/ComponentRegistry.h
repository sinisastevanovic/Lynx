#pragma once

#include "Core.h"
#include <entt/entt.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <functional>
#include <vector>
#include <map>

namespace Lynx
{
    using AddComponentFunc = std::function<void(entt::registry&, entt::entity)>;
    using HasComponentFunc = std::function<bool(entt::registry&, entt::entity)>;
    using RemoveComponentFunc = std::function<void(entt::registry&, entt::entity)>;
    using DrawComponentUIFunc = std::function<void(entt::registry&, entt::entity)>;
    using SerializeComponentFunc = std::function<void(entt::registry&, entt::entity, nlohmann::json&)>;
    using DeserializeComponentFunc = std::function<void(entt::registry&, entt::entity, const nlohmann::json&)>;

    struct LX_API ComponentInfo
    {
        std::string name;
        AddComponentFunc add;
        HasComponentFunc has;
        RemoveComponentFunc remove;
        DrawComponentUIFunc drawUI;
        SerializeComponentFunc serialize;
        DeserializeComponentFunc deserialize;
    };

    class LX_API ComponentRegistry
    {
    public:
        template<typename T>
        void RegisterComponent(const std::string& name, DrawComponentUIFunc uiFunc, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc)
        {
            ComponentInfo info;
            info.name = name;

            info.add = [](entt::registry& registry, entt::entity entity)
            {
                registry.emplace_or_replace<T>(entity);
            };

            info.remove = [](entt::registry& registry, entt::entity entity)
            {
                registry.remove<T>(entity);  
            };

            info.has = [](entt::registry& registry, entt::entity entity)
            {
                return registry.all_of<T>(entity);
            };

            info.drawUI = uiFunc;
            info.serialize = serFunc;
            info.deserialize = deserFunc;

            m_ComponentInfo[name] = info;
        }
    
        template<typename T>
        void RegisterComponent(const std::string& name, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc)
        {
            ComponentInfo info;
            info.name = name;

            info.add = [](entt::registry& registry, entt::entity entity)
            {
                registry.emplace_or_replace<T>(entity);
            };

            info.has = [](entt::registry& registry, entt::entity entity)
            {
                return registry.all_of<T>(entity);
            };

            info.drawUI = [name](entt::registry& registry, entt::entity entity)
            {
                /*if (ImGui::CollapsingHeader(name.c_str()))
                {
                    ImGui::Text("UI for this component not implemented yet.");
                }*/
            };

            info.serialize = serFunc;
            info.deserialize = deserFunc;

            m_ComponentInfo[name] = info;
        }

        const std::map<std::string, ComponentInfo>& GetRegisteredComponents() const
        {
            return m_ComponentInfo;
        }

    private:
        std::map<std::string, ComponentInfo> m_ComponentInfo;
    
    };
}