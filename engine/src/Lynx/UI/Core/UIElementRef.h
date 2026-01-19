#pragma once
#include "Lynx/UUID.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    template<typename T>
    struct ElementRef
    {
        UUID ID = UUID::Null();
        mutable std::weak_ptr<T> Cached;
        
        ElementRef() = default;
        ElementRef(UUID id) : ID(id) {}
        ElementRef(std::shared_ptr<T> element)
            : ID(element ? element->GetUUID() : UUID::Null()), Cached(element)
        {}
    
        std::shared_ptr<T> Get(Scene* scene) const
        {
            if (auto ptr = Cached.lock())
                return ptr;
            
            if (ID.IsValid() && scene)
            {
                auto el = scene->FindUIElementByID(ID);
                auto casted = std::dynamic_pointer_cast<T>(el);
                Cached = casted;
                return casted;
            }
            
            return nullptr;
        }
        
        bool IsValid() const { return !Cached.expired(); }
        
        operator bool() const { return ID.IsValid(); }
        bool operator==(const ElementRef<T>& other) const { return ID == other.ID; }
        bool operator!=(const ElementRef<T>& other) const { return ID != other.ID; }
    };
}

