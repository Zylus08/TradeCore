#pragma once

#include <cstdint>

namespace tradecore::matching {

// Immutable trade representation. Generated once, never modified.
struct alignas(32) Trade {
    uint64_t trade_id;
    uint64_t aggressor_order_id;
    uint64_t resting_order_id;
    uint32_t price;
    uint32_t quantity;
    uint64_t timestamp_ns;
};

static_assert(sizeof(Trade) == 40, "Trade should be compactly packed");

} // namespace tradecore::matching
