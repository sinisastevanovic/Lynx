#pragma once

#include "Lynx/UI/Core/UIElement.h"
#include <queue>
#include <functional>

namespace Lynx
{
    template<typename T>
    class UIObjectPool
    {
    public:
        using FactoryFn = std::function<std::shared_ptr<T>()>;
        
        UIObjectPool(FactoryFn factory, size_t initialSize = 0)
            : m_Factory(factory)
        {
            for (size_t i = 0; i < initialSize; i++)
            {
                auto obj = m_Factory();
                obj->SetVisibility(UIVisibility::Collapsed);
                m_Pool.push(obj);
            }
        }
        
        std::shared_ptr<T> Get()
        {
            std::shared_ptr<T> obj;
            if (m_Pool.empty())
            {
                obj = m_Factory();
            }
            else
            {
                obj = m_Pool.front();
                m_Pool.pop();
            }
            
            obj->SetVisibility(UIVisibility::Visible);
            return obj;
        }
        
        void Return(std::shared_ptr<T> obj)
        {
            obj->SetVisibility(UIVisibility::Collapsed);
            m_Pool.push(obj);
        }
        
    private:
        std::queue<std::shared_ptr<T>> m_Pool;
        FactoryFn m_Factory;
    };
}
