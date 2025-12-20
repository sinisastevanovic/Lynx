#include "lxpch.h"
#include "UUID.h"
#include <random>

namespace Lynx
{
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_Engine(s_RandomDevice());
    static std::uniform_int_distribution<uint64_t> s_UniformDistribution;
    
    UUID::UUID()
        : m_UUID(s_UniformDistribution(s_Engine))
    {
        while (m_UUID == 0)
        {
            m_UUID = s_UniformDistribution(s_Engine);
        }
    }

    UUID::UUID(uint64_t value)
        : m_UUID(value)
    {
    }

    const UUID& UUID::Null()
    {
        static const UUID nullId(0);
        return nullId;
    }

    std::string UUID::ToString() const
    {
        return fmt::format("{:x}", m_UUID);
    }

    UUID UUID::FromString(const std::string& str)
    {
        try
        {
            return UUID(std::stoull(str, nullptr, 16));
        }
        catch (std::exception& ex)
        {
            LX_CORE_ERROR("Error parsing UUID string {0}", str);
            return UUID::Null();
        }
    }
}
