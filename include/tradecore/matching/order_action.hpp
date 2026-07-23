#pragma once

#include <cstdint>

namespace tradecore::matching {

enum class OrderType : uint8_t {
    Limit,
    Market,
    IOC,
    FOK
};

struct alignas(32) OrderAction {
    uint64_t order_id;
    uint64_t client_id;
    uint32_t price;
    uint32_t qty;
    uint8_t  side; // 0 = Buy, 1 = Sell
    OrderType type;
};

} // namespace tradecore::matching
