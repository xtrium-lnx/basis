#pragma once

#include <cstdint>
#include <string_view>

using string_hash_t = std::uint64_t;

namespace basis
{
    // FNV-1a 64-bit
    constexpr string_hash_t hash_string(std::string_view s) noexcept
    {
        string_hash_t hash = 0xcbf29ce484222325ull;
        for (unsigned char c : s)
            hash = (hash ^ c) * 0x00000100000001b3ull;
        return hash;
    }
}

constexpr string_hash_t operator""_h(const char* s, std::size_t n) noexcept
{
    return basis::hash_string({ s, n });
}
