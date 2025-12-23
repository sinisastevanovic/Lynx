#include "ScriptRegistry.h"

namespace Lynx
{
    std::unordered_map<std::string, ScriptRegistry::BindScriptFunc> ScriptRegistry::s_ScriptBindMap;
    std::unordered_map<std::type_index, std::string> ScriptRegistry::s_TypeToNameMap;

    std::string NativeScriptComponent::GetScriptName(std::type_index type)
    {
        auto& scriptNameMap = ScriptRegistry::GetScriptNameMap();
        if (scriptNameMap.find(type) != scriptNameMap.end())
        {
            return scriptNameMap.at(type);
        }
        return std::string();
    }
}