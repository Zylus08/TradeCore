#pragma once

#include "tradecore/common/types.hpp"
#include "tradecore/common/constants.hpp"
#include <cstdint>

namespace tradecore::orderbook {

// State of an order in the book
enum class OrderState : uint8_t {
    Active = 0,
    Partial = 1,
    Filled = 2,
    Canceled = 3
};

// Hot-path struct. Exactly one 64-byte cache line.
struct alignas(64) Order {
    uint64_t order_id;          // offset  0
    uint64_t timestamp_ns;      // offset  8  (insertion time for FIFO)
    uint32_t price;             // offset 16  (fixed-point)
    uint32_t qty;               // offset 20  (original)
    uint32_t remaining_qty;     // offset 24
    uint32_t filled_qty;        // offset 28
    uint32_t next_in_level;     // offset 32  (pool index, intrusive list)
    uint32_t prev_in_level;     // offset 36  (pool index, intrusive list)
    uint16_t symbol;            // offset 40
    uint8_t  side;              // offset 42  (0=Buy, 1=Sell)
    uint8_t  order_type;        // offset 43
    OrderState state;           // offset 44
    uint8_t  flags;             // offset 45
    uint8_t  _pad[18];          // offset 46 -> total = 64 bytes
};
static_assert(sizeof(Order) == 64, "Order must be exactly one cache line");
static_assert(alignof(Order) == 64);

} // namespace tradecore::orderbook
