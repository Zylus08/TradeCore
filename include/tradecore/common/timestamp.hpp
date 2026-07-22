#pragma once

#include <cstdint>
#include <compare>

namespace tradecore {

class Timestamp {
public:
    constexpr Timestamp() noexcept = default;
    constexpr explicit Timestamp(uint64_t nanos) noexcept : nanos_since_midnight_(nanos) {}

    [[nodiscard]] constexpr uint64_t nanos() const noexcept { return nanos_since_midnight_; }

    constexpr auto operator<=>(const Timestamp&) const noexcept = default;

private:
    uint64_t nanos_since_midnight_{0};
};

} // namespace tradecore
