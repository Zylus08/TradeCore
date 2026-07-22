#pragma once

#include <bit>
#include <cstdint>

namespace tradecore {

// Convert network byte order (big-endian) to host byte order
template <typename T>
constexpr T net_to_host(T value) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(value);
    } else {
        return value;
    }
}

// Convert host byte order to network byte order (big-endian)
template <typename T>
constexpr T host_to_net(T value) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(value);
    } else {
        return value;
    }
}

} // namespace tradecore
