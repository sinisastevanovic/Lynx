#pragma once

#include "Entity.h"

namespace Lynx
{
    class ScriptableEntity
    {
    public:
        virtual ~ScriptableEntity() {}

        template<typename T>
        T& GetComponent()
        {
            return m_Entity.GetComponent<T>();
        }

        virtual void OnCreate() {}
        virtual void OnDestroy() {}
        virtual void OnUpdate(float deltaTime) {}

    private:
        Entity m_Entity;
        friend class Scene;
    };
}