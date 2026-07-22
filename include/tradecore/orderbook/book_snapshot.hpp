#pragma once

#include "tradecore/orderbook/price_level.hpp"
#include "tradecore/common/constants.hpp"
#include <span>
#include <cstdint>

namespace tradecore::orderbook {

struct BestBid {
    uint32_t price{0};
    uint32_t qty{0};
    uint32_t order_count{0};
    bool     valid{false};
};

struct BestAsk {
    uint32_t price{0};
    uint32_t qty{0};
    uint32_t order_count{0};
    bool     valid{false};
};

// Immutable, zero-copy view of the order book state at a point in time.
// Consumers (strategy, analytics, replay, visualization) MUST use this.
// They never access internal book structures directly.
//
// Lifetimes: bids/asks spans are valid only as long as the owning OrderBook
// lives and has not been mutated. Callers must not cache across mutations.
struct BookSnapshot {
    uint16_t symbol{0};
    uint64_t sequence_number{0};         // From MoldUDP64 / replay
    uint64_t timestamp_ns{0};            // Timestamp of last mutation

    BestBid  best_bid{};
    BestAsk  best_ask{};

    std::span<const PriceLevel> bids{};  // Descending order
    std::span<const PriceLevel> asks{};  // Ascending order

    // Derived helpers — branchless, inline
    [[nodiscard]] constexpr bool has_bbo() const noexcept {
        return best_bid.valid && best_ask.valid;
    }

    [[nodiscard]] constexpr uint32_t spread() const noexcept {
        if (!has_bbo()) return 0;
        return best_ask.price - best_bid.price;
    }

    [[nodiscard]] constexpr uint32_t mid_price() const noexcept {
        if (!has_bbo()) return 0;
        return (best_bid.price + best_ask.price) / 2;
    }

    [[nodiscard]] constexpr std::size_t bid_depth() const noexcept {
        return bids.size();
    }

    [[nodiscard]] constexpr std::size_t ask_depth() const noexcept {
        return asks.size();
    }
};

// Concept: any module that observes book state must accept BookSnapshot.
// This enforces the read-only boundary at compile time.
template <typename T>
concept BookObserver = requires(T obs, const BookSnapshot& snap) {
    { obs.on_book_update(snap) } -> std::same_as<void>;
};

} // namespace tradecore::orderbook
