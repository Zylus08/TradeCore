#pragma once

#include "tradecore/common/types.hpp"
#include <cstdint>

namespace tradecore::parser {

// Standardized enum independent of exchange
enum class UpdateType : uint8_t {
    AddOrder = 1,
    ModifyOrder = 2,
    CancelOrder = 3,
    DeleteOrder = 4,
    ExecuteOrder = 5,
    ExecuteOrderWithPrice = 6,
    Trade = 7,
    SystemEvent = 8
};

// Protocol-independent internal format (canonical representation)
struct alignas(64) BookUpdate {
    UpdateType type;
    uint8_t side;           // 0: Buy, 1: Sell
    uint16_t symbol;
    uint32_t price;         // Fixed-point
    uint32_t qty;
    uint32_t remaining_qty; // Used for partial executes
    uint64_t timestamp_ns;
    uint64_t order_id;
    uint64_t match_id;      // Used for trades/executions
    uint8_t flags;
    uint8_t _pad[23];       // Exact 64-byte alignment
};
static_assert(sizeof(BookUpdate) == 64, "BookUpdate must be exactly one cache line");

} // namespace tradecore::parser
