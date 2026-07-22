#pragma once

#include "tradecore/common/constants.hpp"
#include <cstdint>

namespace tradecore::orderbook {

// Hot-path struct. Exactly one 64-byte cache line.
struct alignas(64) PriceLevel {
    uint32_t price;             // offset  0  (fixed-point)
    uint32_t total_qty;         // offset  4  (sum of all order remaining_qty)
    uint32_t order_count;       // offset  8
    uint32_t head_order_idx;    // offset 12  (pool index, FIFO front)
    uint32_t tail_order_idx;    // offset 16  (pool index, FIFO back)
    uint32_t left_child;        // offset 20  (AVL tree: pool index)
    uint32_t right_child;       // offset 24  (AVL tree: pool index)
    uint32_t parent;            // offset 28  (AVL tree: pool index)
    int8_t   balance;           // offset 32  (AVL balance factor: -1, 0, +1)
    uint8_t  side;              // offset 33  (0=Bid, 1=Ask)
    uint8_t  _pad[30];          // offset 34 -> total = 64 bytes
};
static_assert(sizeof(PriceLevel) == 64, "PriceLevel must be exactly one cache line");
static_assert(alignof(PriceLevel) == 64);

} // namespace tradecore::orderbook
