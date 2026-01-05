#pragma once

#include <nvrhi/nvrhi.h>
#include <unordered_map>
#include <functional>

namespace Lynx
{
    template <typename KeyType>
    class BindingSetCache
    {
    public:
        using CreatorFunc = std::function<nvrhi::BindingSetHandle()>;

        nvrhi::BindingSetHandle Get(KeyType key, uint32_t version, CreatorFunc creator)
        {
            auto it = m_Cache.find(key);
            if (it != m_Cache.end())
            {
                if (it->second.Version == version)
                    return it->second.BindingSet;
            }

            nvrhi::BindingSetHandle newSet = creator();
            if (newSet)
            {
                m_Cache[key] = { newSet, version };
            }
            return newSet;
        }

        void Clear()
        {
            m_Cache.clear();
        }

        void Invalidate(KeyType key)
        {
            m_Cache.erase(key);
        }

    private:
        struct Entry
        {
            nvrhi::BindingSetHandle BindingSet;
            uint32_t Version;
        };

        std::unordered_map<KeyType, Entry> m_Cache;
    };
}
