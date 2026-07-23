#pragma once

#include <cstddef>
#include <cstdint>

#include <new>

namespace tradecore {

// Hardware Constants
constexpr std::size_t kCacheLineSize = 64;
constexpr std::size_t kHugePageSize = 2 * 1024 * 1024; // 2MB

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t kHardwareDestructiveInterferenceSize = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t kHardwareDestructiveInterferenceSize = 64;
#endif

// Application Constraints
constexpr uint32_t kMaxSymbols = 65536;
constexpr uint32_t kPriceMultiplier = 10000;

// Sentinel Values
constexpr uint32_t kInvalidPoolIndex = static_cast<uint32_t>(-1);
constexpr uint16_t kInvalidSymbol = static_cast<uint16_t>(-1);

} // namespace tradecore
