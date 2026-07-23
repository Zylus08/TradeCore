#pragma once

#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <array>
#include <cstdint>
#include <limits>

namespace tradecore::orderbook {

// FlatPriceArray is a dense array structure designed to replace or augment AVL trees 
// for top-of-book (BBO) tracking. By maintaining an array of price offsets, it 
// provides O(1) access to PriceLevel indices without pointer chasing.
//
// This is typically bounded around a daily reference price or dynamic anchor,
// but for a pure flat representation, it maps Tick offsets to Level Indices.
template <std::size_t MaxTicks = 10000>
class FlatPriceArray {
public:
    FlatPriceArray() {
        level_indices_.fill(kInvalidPoolIndex);
    }

    // Set the anchor price (e.g., open price or 0 for direct offset)
    void set_anchor(uint32_t anchor_price, uint32_t tick_size = 1) noexcept {
        anchor_price_ = anchor_price;
        tick_size_ = tick_size;
    }

    [[nodiscard]] uint32_t find(uint32_t price) const noexcept {
        std::size_t idx = price_to_index(price);
        if (TRADECORE_UNLIKELY(idx >= MaxTicks)) return kInvalidPoolIndex;
        return level_indices_[idx];
    }

    void insert(uint32_t price, uint32_t level_pool_idx) noexcept {
        std::size_t idx = price_to_index(price);
        TRADECORE_ASSERT(idx < MaxTicks);
        level_indices_[idx] = level_pool_idx;
        update_bbo_insert(idx);
    }

    void remove(uint32_t price) noexcept {
        std::size_t idx = price_to_index(price);
        TRADECORE_ASSERT(idx < MaxTicks);
        level_indices_[idx] = kInvalidPoolIndex;
        update_bbo_remove(idx);
    }

    [[nodiscard]] uint32_t best_index() const noexcept {
        return best_idx_ == kInvalidPoolIndex ? kInvalidPoolIndex : level_indices_[best_idx_];
    }

    [[nodiscard]] uint32_t best_price() const noexcept {
        return best_idx_ == kInvalidPoolIndex ? 0 : anchor_price_ + (best_idx_ * tick_size_);
    }

private:
    [[nodiscard]] std::size_t price_to_index(uint32_t price) const noexcept {
        if (price < anchor_price_) return std::numeric_limits<std::size_t>::max();
        return (price - anchor_price_) / tick_size_;
    }

    void update_bbo_insert(std::size_t idx) noexcept {
        if (best_idx_ == kInvalidPoolIndex || is_better(idx, best_idx_)) {
            best_idx_ = idx;
        }
    }

    void update_bbo_remove(std::size_t idx) noexcept {
        if (idx == best_idx_) {
            // Recompute BBO (scan dense array)
            // In a real Bid/Ask array, the scan direction changes based on side.
            // For simplicity in this scaffold, we just scan for the next valid index.
            // This is O(N) in worst case but extremely fast over L1 cache.
            best_idx_ = kInvalidPoolIndex;
            for (std::size_t i = 0; i < MaxTicks; ++i) {
                if (level_indices_[i] != kInvalidPoolIndex) {
                    if (best_idx_ == kInvalidPoolIndex || is_better(i, best_idx_)) {
                        best_idx_ = i;
                    }
                }
            }
        }
    }

    // Bid vs Ask comparison logic should be injected or templated,
    // assuming Bid (higher is better) for this baseline scaffold.
    [[nodiscard]] bool is_better(std::size_t a, std::size_t b) const noexcept {
        return a > b; 
    }

    alignas(kHardwareDestructiveInterferenceSize) std::array<uint32_t, MaxTicks> level_indices_;
    uint32_t anchor_price_{0};
    uint32_t tick_size_{1};
    std::size_t best_idx_{kInvalidPoolIndex};
};

} // namespace tradecore::orderbook
