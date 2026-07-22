#pragma once

#include "tradecore/orderbook/price_level.hpp"
#include "tradecore/orderbook/price_level_pool.hpp"
#include "tradecore/orderbook/book_snapshot.hpp"
#include "tradecore/orderbook/avl_tree.hpp"
#include "tradecore/orderbook/avl_statistics.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <cstdint>

namespace tradecore::orderbook {

// BookSide owns the price-level AVL tree for one side of the book (bid or ask).
// Phase 3B: full price-level lifecycle, no order operations yet.
template <std::size_t MaxLevels>
class BookSide {
public:
    using Index = uint32_t;
    static constexpr Index kInvalid = kInvalidPoolIndex;

    explicit BookSide(uint8_t side) noexcept : side_(side) {}

    // ---- Phase 3B: Level Lifecycle -----------------------------------------------

    // Step 1: Allocate and initialize a level node from the pool.
    // Does NOT insert into the AVL tree yet.
    [[nodiscard]] Index create_level(uint32_t price,
                                     PriceLevelPool<MaxLevels>& pool) noexcept {
        PriceLevel* lvl = pool.allocate();
        if (TRADECORE_UNLIKELY(lvl == nullptr)) return kInvalid;

        lvl->price          = price;
        lvl->total_qty      = 0;
        lvl->order_count    = 0;
        lvl->head_order_idx = kInvalid;
        lvl->tail_order_idx = kInvalid;
        lvl->left_child     = kInvalid;
        lvl->right_child    = kInvalid;
        lvl->parent         = kInvalid;
        lvl->balance        = 0;
        lvl->side           = side_;

        return pool.index_of(lvl);
    }

    // Step 2: Insert a created level into the AVL tree and update BBO.
    void insert_level(Index idx, PriceLevelPool<MaxLevels>& pool,
                      AVLStatistics& stats) noexcept {
        avl_root_ = avl::insert(pool, avl_root_, idx, stats);
        ++level_count_;
        update_bbo_on_insert(pool, idx);
        avl::update_stats_height(pool, avl_root_, stats);
    }

    // Step 3: Remove a level from the AVL tree and update BBO.
    void remove_level(Index idx, PriceLevelPool<MaxLevels>& pool,
                      AVLStatistics& stats) noexcept {
        TRADECORE_ASSERT(level_count_ > 0);
        const bool was_bbo = (idx == bbo_idx_);

        avl_root_ = avl::remove(pool, avl_root_, idx, stats);
        --level_count_;

        if (was_bbo) {
            // BBO removed: find new BBO from tree
            bbo_idx_ = (side_ == 0)
                       ? avl::find_max(pool, avl_root_)  // bids: max price
                       : avl::find_min(pool, avl_root_); // asks: min price
        }
        avl::update_stats_height(pool, avl_root_, stats);
    }

    // Step 4: Return pool slot. Must be called after remove_level.
    void destroy_level(Index idx, PriceLevelPool<MaxLevels>& pool) noexcept {
        pool.deallocate(pool.at(idx));
    }

    // Combined convenience: remove from tree AND free pool slot.
    void remove_and_destroy_level(Index idx, PriceLevelPool<MaxLevels>& pool,
                                  AVLStatistics& stats) noexcept {
        remove_level(idx, pool, stats);
        destroy_level(idx, pool);
    }

    // ---- Read-only accessors -------------------------------------------------------

    [[nodiscard]] uint32_t avl_root() const noexcept { return avl_root_; }
    [[nodiscard]] uint32_t bbo_idx()  const noexcept { return bbo_idx_; }
    [[nodiscard]] bool     has_bbo()  const noexcept { return bbo_idx_ != kInvalid; }
    [[nodiscard]] std::size_t level_count() const noexcept { return level_count_; }
    [[nodiscard]] uint8_t  side()     const noexcept { return side_; }

    // O(1): returns BBO level directly from cached index
    [[nodiscard]] const PriceLevel* bbo_level(const PriceLevelPool<MaxLevels>& pool) const noexcept {
        if (bbo_idx_ == kInvalid) return nullptr;
        return pool.at(bbo_idx_);
    }

    // Find level by price — O(log N)
    [[nodiscard]] Index find_by_price(const PriceLevelPool<MaxLevels>& pool,
                                      uint32_t price) const noexcept {
        return avl::find(pool, avl_root_, price);
    }

    // Snapshot helpers
    [[nodiscard]] BestBid best_bid(const PriceLevelPool<MaxLevels>& pool) const noexcept {
        TRADECORE_ASSERT(side_ == 0);
        if (bbo_idx_ == kInvalid) return BestBid{};
        const auto* lvl = pool.at(bbo_idx_);
        return BestBid{lvl->price, lvl->total_qty, lvl->order_count, true};
    }

    [[nodiscard]] BestAsk best_ask(const PriceLevelPool<MaxLevels>& pool) const noexcept {
        TRADECORE_ASSERT(side_ == 1);
        if (bbo_idx_ == kInvalid) return BestAsk{};
        const auto* lvl = pool.at(bbo_idx_);
        return BestAsk{lvl->price, lvl->total_qty, lvl->order_count, true};
    }

private:
    // Incremental BBO update on INSERT — O(1) amortized
    void update_bbo_on_insert(const PriceLevelPool<MaxLevels>& pool,
                              Index new_idx) noexcept {
        if (bbo_idx_ == kInvalid) {
            bbo_idx_ = new_idx;
            return;
        }
        const uint32_t new_price = pool.at(new_idx)->price;
        const uint32_t bbo_price = pool.at(bbo_idx_)->price;
        if (side_ == 0) {
            if (new_price > bbo_price) bbo_idx_ = new_idx; // bids: higher is better
        } else {
            if (new_price < bbo_price) bbo_idx_ = new_idx; // asks: lower is better
        }
    }

    uint32_t    avl_root_{kInvalid};
    uint32_t    bbo_idx_{kInvalid};
    std::size_t level_count_{0};
    uint8_t     side_;
};

} // namespace tradecore::orderbook
