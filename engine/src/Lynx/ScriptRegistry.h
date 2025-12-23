#pragma once
#include <typeindex>

#include "Lynx/Scene/Components/NativeScriptComponent.h"

namespace Lynx
{
    class LX_API ScriptRegistry
    {
    public:
        using BindScriptFunc = std::function<void(NativeScriptComponent&)>;

        template<typename T>
        static void RegisterScript(const std::string& name)
        {
            s_ScriptBindMap[name] = [](NativeScriptComponent& nsc)
            {
                nsc.Bind<T>();
            };
            s_TypeToNameMap[std::type_index(typeid(T))] = name;
        }

        static void Bind(const std::string& name, NativeScriptComponent& nsc)
        {
            if (s_ScriptBindMap.find(name) != s_ScriptBindMap.end())
            {
                s_ScriptBindMap[name](nsc);
                nsc.ScriptName = name;
            }
            else
            {
                LX_CORE_ERROR("Script not found in registry: {0}", name);
            }
        }

        template<typename T>
        static void Bind(NativeScriptComponent& nsc)
        {
            nsc.Bind<T>();
            auto it = s_TypeToNameMap.find(std::type_index(typeid(T)));
            if (it != s_TypeToNameMap.end())
            {
                nsc.ScriptName = it->second;
            }
            else
            {
                LX_CORE_ERROR("Script type binding failed to find name in registry! Did you forget to call RegisterScript?");
            }
        }

        static const std::unordered_map<std::string, BindScriptFunc>& GetRegisteredScripts() { return s_ScriptBindMap; }
        static const std::unordered_map<std::type_index, std::string>& GetScriptNameMap() { return s_TypeToNameMap; }

    private:
        static std::unordered_map<std::string, BindScriptFunc> s_ScriptBindMap;
        static std::unordered_map<std::type_index, std::string> s_TypeToNameMap;
    };
}
