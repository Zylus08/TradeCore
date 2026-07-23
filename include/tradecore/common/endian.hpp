#pragma once

#include <cstdint>
#include <type_traits>

namespace tradecore {

namespace detail {
    inline uint16_t bswap(uint16_t x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_bswap16(x);
#elif defined(_MSC_VER)
        return _byteswap_ushort(x);
#else
        return static_cast<uint16_t>((x << 8) | (x >> 8));
#endif
    }

    inline uint32_t bswap(uint32_t x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_bswap32(x);
#elif defined(_MSC_VER)
        return _byteswap_ulong(x);
#else
        return (x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24);
#endif
    }

    inline uint64_t bswap(uint64_t x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_bswap64(x);
#elif defined(_MSC_VER)
        return _byteswap_uint64(x);
#else
        return (static_cast<uint64_t>(bswap(static_cast<uint32_t>(x))) << 32) |
               bswap(static_cast<uint32_t>(x >> 32));
#endif
    }
} // namespace detail

// Determine endianness at compile time (fallback for pre-C++20, though we target C++20)
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    inline constexpr bool kIsBigEndian = true;
#else
    inline constexpr bool kIsBigEndian = false;
#endif

// Convert network byte order (big-endian) to host byte order using inlined intrinsics
template <typename T>
    requires std::is_integral_v<T>
inline T net_to_host(T value) noexcept {
    if constexpr (!kIsBigEndian) {
        if constexpr (sizeof(T) == 2) return detail::bswap(static_cast<uint16_t>(value));
        else if constexpr (sizeof(T) == 4) return detail::bswap(static_cast<uint32_t>(value));
        else if constexpr (sizeof(T) == 8) return detail::bswap(static_cast<uint64_t>(value));
        else return value;
    } else {
        return value;
    }
}

// Convert host byte order to network byte order (big-endian)
template <typename T>
    requires std::is_integral_v<T>
inline T host_to_net(T value) noexcept {
    return net_to_host(value); // Symmetrical operation
}

} // namespace tradecore
