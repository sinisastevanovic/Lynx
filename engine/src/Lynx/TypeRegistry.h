#pragma once

#include "Core.h"
#include <entt/entt.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <functional>
#include <vector>
#include <map>

#include "Scene/Components/NativeScriptComponent.h"

namespace Lynx
{
    using AddComponentFunc = std::function<void(entt::registry&, entt::entity)>;
    using HasComponentFunc = std::function<bool(entt::registry&, entt::entity)>;
    using RemoveComponentFunc = std::function<void(entt::registry&, entt::entity)>;
    using DrawComponentUIFunc = std::function<void(entt::registry&, entt::entity)>;
    using SerializeComponentFunc = std::function<void(entt::registry&, entt::entity, nlohmann::json&)>;
    using DeserializeComponentFunc = std::function<void(entt::registry&, entt::entity, const nlohmann::json&)>;

    struct ComponentInfo
    {
        std::string name;
        AddComponentFunc add;
        HasComponentFunc has;
        RemoveComponentFunc remove;
        DrawComponentUIFunc drawUI;
        SerializeComponentFunc serialize;
        DeserializeComponentFunc deserialize;
        bool IsCore;
    };

    using ScriptBindFunc = std::function<void(NativeScriptComponent&)>;

    struct ScriptInfo
    {
        ScriptBindFunc Bind;
        bool IsCore = false;
    };

    class TypeRegistry
    {
    public:
        template <typename T>
        void RegisterCoreComponent(const std::string& name, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc, DrawComponentUIFunc uiFunc)
        {
            RegisterInternal<T>(name, serFunc, deserFunc, uiFunc, true);
        }

        template <typename T>
        void RegisterCoreComponent(const std::string& name, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc)
        {
            RegisterInternal<T>(name, serFunc, deserFunc, nullptr, true);
        }

        template <typename T>
        void RegisterCoreScript(const std::string& name)
        {
            RegisterScriptInternal<T>(name, true);
        }
        
        void BindScript(const std::string& name, NativeScriptComponent& nsc)
        {
            if (m_RegisteredScripts.find(name) != m_RegisteredScripts.end())
            {
                m_RegisteredScripts[name].Bind(nsc);
                nsc.ScriptName = name;
            }
            else
            {
                LX_CORE_ERROR("Script not found in registry: {0}", name);
            }
        }

        template<typename T>
        void BindScript(NativeScriptComponent& nsc)
        {
            nsc.Bind<T>();
            auto it = m_TypeToNameMap.find(std::type_index(typeid(T)));
            if (it != m_TypeToNameMap.end())
            {
                nsc.ScriptName = it->second;
            }
            else
            {
                LX_CORE_ERROR("Script type binding failed to find name in registry! Did you forget to call RegisterScript?");
            }
        }

        void ClearGameComponents()
        {
            std::erase_if(m_RegisteredComponents, [](const auto& item)
            {
                return !item.second.IsCore;
            });
        }
        
        void ClearGameScripts()
        {
            std::erase_if(m_RegisteredScripts, [](const auto& item)
            {
                return !item.second.IsCore;
            });
        }

        void ClearAll()
        {
            m_RegisteredComponents.clear();
            m_RegisteredScripts.clear();
            m_TypeToNameMap.clear();
        }

        const std::map<std::string, ComponentInfo>& GetRegisteredComponents() const
        {
            return m_RegisteredComponents;
        }
        
        const std::unordered_map<std::string, ScriptInfo>& GetRegisteredScripts() { return m_RegisteredScripts; }
        const std::unordered_map<std::type_index, std::string>& GetScriptNameMap() { return m_TypeToNameMap; }
        
        std::string GetScriptName(std::type_index type)
        {
            if (m_TypeToNameMap.find(type) != m_TypeToNameMap.end())
            {
                return m_TypeToNameMap.at(type);
            }
            return std::string();
        }

        friend class GameTypeRegistry;

    private:
        template <typename T>
        void RegisterInternal(const std::string& name, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc, DrawComponentUIFunc uiFunc,
                              bool isCore)
        {
            ComponentInfo info;
            info.name = name;

            info.add = [](entt::registry& registry, entt::entity entity)
            {
                registry.emplace_or_replace<T>(entity);
            };

            info.remove = [](entt::registry& registry, entt::entity entity)
            {
                if (registry.all_of<T>(entity))
                    registry.remove<T>(entity);
            };

            info.has = [](entt::registry& registry, entt::entity entity)
            {
                return registry.all_of<T>(entity);
            };

            info.drawUI = uiFunc;
            info.serialize = serFunc;
            info.deserialize = deserFunc;
            info.IsCore = isCore;

            m_RegisteredComponents[name] = info;
        }

        template <typename T>
        void RegisterScriptInternal(const std::string& name, bool isCore)
        {
            ScriptInfo info;
            info.IsCore = isCore;
            info.Bind = [](NativeScriptComponent& nsc)
            {
                nsc.Bind<T>();
            };
            m_RegisteredScripts[name] = info;
            m_TypeToNameMap[std::type_index(typeid(T))] = name;
        }

    private:
        std::map<std::string, ComponentInfo> m_RegisteredComponents;
        std::unordered_map<std::string, ScriptInfo> m_RegisteredScripts;
        std::unordered_map<std::type_index, std::string> m_TypeToNameMap;
    };

    class GameTypeRegistry
    {
    public:
        GameTypeRegistry(TypeRegistry& registry) : m_Registry(registry)
        {
        }

        template <typename T>
        void RegisterComponent(const std::string& name, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc, DrawComponentUIFunc uiFunc)
        {
            m_Registry.RegisterInternal<T>(name, serFunc, deserFunc, uiFunc, false);
        }

        template <typename T>
        void RegisterComponent(const std::string& name, SerializeComponentFunc serFunc, DeserializeComponentFunc deserFunc)
        {
            m_Registry.RegisterInternal<T>(name, serFunc, deserFunc, nullptr, false);
        }
        
        template <typename T>
        void RegisterScript(const std::string& name)
        {
            m_Registry.RegisterScriptInternal<T>(name, false);
        }

    private:
        TypeRegistry& m_Registry;
    };
}
