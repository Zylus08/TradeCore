#pragma once

#include "tradecore/orderbook/order_book.hpp"
#include <cstdint>
#include <span>

namespace tradecore::orderbook {

class BookDigest {
public:
    static constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
    static constexpr uint64_t kFnvPrime = 1099511628211ULL;

    template <std::size_t MaxOrders, std::size_t MaxLevels>
    static uint64_t compute(const OrderBook<MaxOrders, MaxLevels>& book) noexcept {
        uint64_t hash = kFnvOffsetBasis;

        // Traverse bids (highest to lowest)
        hash = hash_side(hash, book.bids(), book.level_pool(), book.order_pool());
        
        // Traverse asks (lowest to highest)
        hash = hash_side(hash, book.asks(), book.level_pool(), book.order_pool());

        return hash;
    }

private:
    static inline uint64_t hash_combine(uint64_t hash, uint64_t val) noexcept {
        // Mix a 64-bit integer into the FNV-1a hash state
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
        for (std::size_t i = 0; i < 8; ++i) {
            hash ^= p[i];
            hash *= kFnvPrime;
        }
        return hash;
    }

    template <std::size_t MaxLevels, std::size_t MaxOrders>
    static uint64_t hash_side(uint64_t hash,
                              const BookSide<MaxLevels>& side,
                              const PriceLevelPool<MaxLevels>& level_pool,
                              const OrderPool<MaxOrders>& order_pool) noexcept {
        
        uint32_t root = side.avl_root();
        if (root == kInvalidPoolIndex) return hash;

        // In-order traversal. For bids, we want descending price (right-to-left).
        // For asks, we want ascending price (left-to-right).
        // We'll define a recursive helper to do this deterministically.
        if (side.side() == 0) {
            hash = traverse_bids(hash, root, level_pool, order_pool);
        } else {
            hash = traverse_asks(hash, root, level_pool, order_pool);
        }
        return hash;
    }

    template <std::size_t MaxLevels, std::size_t MaxOrders>
    static uint64_t traverse_bids(uint64_t hash, uint32_t node_idx,
                                  const PriceLevelPool<MaxLevels>& level_pool,
                                  const OrderPool<MaxOrders>& order_pool) noexcept {
        if (node_idx == kInvalidPoolIndex) return hash;
        const auto* node = level_pool.at(node_idx);
        
        hash = traverse_bids(hash, node->node.right, level_pool, order_pool);
        hash = hash_level(hash, node, order_pool);
        hash = traverse_bids(hash, node->node.left, level_pool, order_pool);
        return hash;
    }

    template <std::size_t MaxLevels, std::size_t MaxOrders>
    static uint64_t traverse_asks(uint64_t hash, uint32_t node_idx,
                                  const PriceLevelPool<MaxLevels>& level_pool,
                                  const OrderPool<MaxOrders>& order_pool) noexcept {
        if (node_idx == kInvalidPoolIndex) return hash;
        const auto* node = level_pool.at(node_idx);
        
        hash = traverse_asks(hash, node->node.left, level_pool, order_pool);
        hash = hash_level(hash, node, order_pool);
        hash = traverse_asks(hash, node->node.right, level_pool, order_pool);
        return hash;
    }

    template <std::size_t MaxOrders>
    static uint64_t hash_level(uint64_t hash, const PriceLevel* lvl,
                               const OrderPool<MaxOrders>& order_pool) noexcept {
        hash = hash_combine(hash, lvl->price);
        hash = hash_combine(hash, static_cast<uint64_t>(lvl->side));
        hash = hash_combine(hash, lvl->total_qty);

        uint32_t curr = lvl->head_order_idx;
        while (curr != kInvalidPoolIndex) {
            const auto* order = order_pool.at(curr);
            hash = hash_combine(hash, order->order_id);
            hash = hash_combine(hash, order->remaining_qty);
            curr = order->next_in_level;
        }
        return hash;
    }
};

} // namespace tradecore::orderbook
