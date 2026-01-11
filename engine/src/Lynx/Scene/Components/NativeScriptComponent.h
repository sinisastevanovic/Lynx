#pragma once
#include <typeindex>

#include "Lynx/Scene/ScriptableEntity.h"

namespace Lynx
{
    // TODO: Add drawImgui functions to ScripableEntity
    struct NativeScriptComponent
    {
        ScriptableEntity* Instance = nullptr;
        std::string ScriptName;

        ScriptableEntity* (*InstantiateScript)();
        void (*DestroyScript)(NativeScriptComponent*);

        static LX_API std::string GetScriptName(std::type_index type);

        template<typename T>
        void Bind()
        {
            InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
            DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
            ScriptName = GetScriptName(std::type_index(typeid(T)));
        }
    };
}
