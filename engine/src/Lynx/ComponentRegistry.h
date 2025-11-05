#pragma once

#include "Core.h"
#include <entt/entt.hpp>

#include <string>
#include <functional>
#include <vector>
#include <map>

namespace Lynx
{
    using AddComponentFunc = std::function<void(entt::registry&, entt::entity)>;
    using HasComponentFunc = std::function<bool(entt::registry&, entt::entity)>;
    using DrawComponentUIFunc = std::function<void(entt::registry&, entt::entity)>;

    struct LX_API ComponentInfo
    {
        std::string name;
        AddComponentFunc add;
        HasComponentFunc has;
        DrawComponentUIFunc drawUI;
    };

    class LX_API ComponentRegistry
    {
    public:
        template<typename T>
        void RegisterComponent(const std::string& name, DrawComponentUIFunc uiFunc)
        {
            ComponentInfo info;
            info.name = name;

            info.add = [](entt::registry& registry, entt::entity entity)
            {
                registry.emplace<T>(entity);
            };

            info.has = [](entt::registry& registry, entt::entity entity)
            {
                return registry.all_of<T>(entity);
            };

            info.drawUI = uiFunc;

            m_ComponentInfo[name] = info;
        }
    
        template<typename T>
        void RegisterComponent(const std::string& name)
        {
            ComponentInfo info;
            info.name = name;

            info.add = [](entt::registry& registry, entt::entity entity)
            {
                registry.emplace<T>(entity);
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