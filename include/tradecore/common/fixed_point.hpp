#pragma once

#include <cstdint>
#include <compare>
#include "tradecore/common/constants.hpp"

namespace tradecore {

class FixedPointPrice {
public:
    constexpr FixedPointPrice() noexcept = default;
    constexpr explicit FixedPointPrice(uint32_t raw_value) noexcept : value_(raw_value) {}

    [[nodiscard]] constexpr uint32_t raw() const noexcept { return value_; }

    constexpr auto operator<=>(const FixedPointPrice&) const noexcept = default;

private:
    uint32_t value_{0};
};

} // namespace tradecore
