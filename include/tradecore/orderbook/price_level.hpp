#pragma once

#include "tradecore/common/constants.hpp"
#include <cstdint>

namespace tradecore::orderbook {

struct AVLNode {
    uint32_t left{kInvalidPoolIndex};
    uint32_t right{kInvalidPoolIndex};
    uint32_t parent{kInvalidPoolIndex};
    int32_t  height{1};
};

// Hot-path struct. Exactly one 64-byte cache line.
struct alignas(64) PriceLevel {
    uint32_t price;             // offset  0  (fixed-point)
    uint32_t total_qty;         // offset  4  (sum of all order remaining_qty)
    uint32_t order_count;       // offset  8
    uint32_t head_order_idx;    // offset 12  (pool index, FIFO front)
    uint32_t tail_order_idx;    // offset 16  (pool index, FIFO back)
    AVLNode  node;              // offset 20  (AVL tree metadata, 16 bytes)
    uint8_t  side;              // offset 36  (0=Bid, 1=Ask)
    uint8_t  _pad[27];          // offset 37 -> total = 64 bytes
};
static_assert(sizeof(PriceLevel) == 64, "PriceLevel must be exactly one cache line");
static_assert(alignof(PriceLevel) == 64);

} // namespace tradecore::orderbook
