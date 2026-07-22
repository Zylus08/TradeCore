#pragma once

#include <cstdint>

namespace tradecore {

// Market Data & Order Book Types
using OrderId = uint64_t;
using Price = uint32_t; // Fixed-point representation (e.g., price * 10000)
using Qty = uint32_t;
using SymbolIndex = uint16_t;

// Enum for Buy/Sell sides
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

// Type of order
enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1,
    IOC = 2,
    FOK = 3
};

// Lifecycle state of an order
enum class OrderState : uint8_t {
    New = 0,
    Active = 1,
    Partial = 2,
    Filled = 3,
    Canceled = 4,
    Rejected = 5
};

} // namespace tradecore
