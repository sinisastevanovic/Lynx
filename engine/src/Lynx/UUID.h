#pragma once

#include "Core.h"
#include "Log.h"

namespace Lynx
{
    class LX_API UUID
    {
    public:
        UUID();
        explicit UUID(uint64_t value);
        UUID(const UUID& other) = default;

        static const UUID& Null();

        bool IsValid() const { return m_UUID != 0; }

        // Convert UUID to a string representation (hex)
        std::string ToString() const;
        UUID FromString(const std::string& str);

        bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
        bool operator!=(const UUID& other) const { return !(*this == other); }
        bool operator<(const UUID& other) const { return m_UUID < other.m_UUID; }
        explicit operator bool() const { return m_UUID != 0; }
        operator uint64_t() const { return m_UUID; }
    private:
        uint64_t m_UUID;
    
    };

    typedef UUID AssetHandle; // TODO: I think we should refactor AssetHandle into it's own class with helper methods to Load the asset async or blocking...
                                // Maybe with a .Get() method and .IsLoaded() and .Load() and .LoadAsync()
}

namespace std
{
    template <typename T> struct hash;

    template<>
    struct hash<Lynx::UUID>
    {
        std::size_t operator()(const Lynx::UUID& uuid) const
        {
            return std::hash<uint64_t>()((uint64_t)uuid);
        }
    };
}

inline std::ostream& operator<<(std::ostream& os, const Lynx::UUID& uuid)
{
    return os << uuid.ToString();
}

template<>
struct fmt::formatter<Lynx::UUID>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}')
        {
            throw format_error("invalid format");
        }
        return it;
    }

    template<typename FormatContext>
    auto format(const Lynx::UUID& uuid, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", (uint64_t)uuid);
    }
};