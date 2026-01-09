#include "NativeScriptComponent.h"

#include "Lynx/Engine.h"

namespace Lynx
{
    std::string NativeScriptComponent::GetScriptName(std::type_index type)
    {
        auto& scriptNameMap = Engine::Get().GetComponentRegistry().GetScriptNameMap();
        if (scriptNameMap.find(type) != scriptNameMap.end())
        {
            return scriptNameMap.at(type);
        }
        return std::string();
    }
}
