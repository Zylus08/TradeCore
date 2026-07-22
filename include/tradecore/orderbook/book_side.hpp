#pragma once

#include "tradecore/orderbook/price_level.hpp"
#include "tradecore/orderbook/price_level_pool.hpp"
#include "tradecore/orderbook/book_snapshot.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace tradecore::orderbook {

// BookSide holds one half of the order book (bids OR asks).
// Read-only accessors only in Phase 3A.
// Mutation logic (AVL insert/delete, BBO update) added in Phase 3B.
template <std::size_t MaxLevels>
class BookSide {
public:
    using Index = uint32_t;
    static constexpr Index kInvalid = kInvalidPoolIndex;

    explicit BookSide(uint8_t side) noexcept : side_(side) {}

    // ---- Read-only accessors ----

    [[nodiscard]] uint32_t avl_root() const noexcept {
        return avl_root_idx_;
    }

    [[nodiscard]] uint32_t bbo_idx() const noexcept {
        return bbo_idx_;
    }

    [[nodiscard]] bool has_bbo() const noexcept {
        return bbo_idx_ != kInvalid;
    }

    [[nodiscard]] const PriceLevel* bbo(const PriceLevelPool<MaxLevels>& pool) const noexcept {
        if (bbo_idx_ == kInvalid) return nullptr;
        return pool.at(bbo_idx_);
    }

    [[nodiscard]] std::size_t level_count() const noexcept {
        return level_count_;
    }

    [[nodiscard]] uint8_t side() const noexcept {
        return side_;
    }

    // Produce the BestBid or BestAsk portion of a BookSnapshot
    [[nodiscard]] BestBid best_bid(const PriceLevelPool<MaxLevels>& pool) const noexcept {
        TRADECORE_ASSERT(side_ == 0); // only valid on bid side
        if (bbo_idx_ == kInvalid) return BestBid{};
        const auto* lvl = pool.at(bbo_idx_);
        return BestBid{lvl->price, lvl->total_qty, lvl->order_count, true};
    }

    [[nodiscard]] BestAsk best_ask(const PriceLevelPool<MaxLevels>& pool) const noexcept {
        TRADECORE_ASSERT(side_ == 1); // only valid on ask side
        if (bbo_idx_ == kInvalid) return BestAsk{};
        const auto* lvl = pool.at(bbo_idx_);
        return BestAsk{lvl->price, lvl->total_qty, lvl->order_count, true};
    }

protected:
    // Mutable state — exposed to Phase 3B subclass / friend operations
    uint32_t    avl_root_idx_{kInvalid};
    uint32_t    bbo_idx_{kInvalid};
    std::size_t level_count_{0};
    uint8_t     side_;
};

} // namespace tradecore::orderbook
