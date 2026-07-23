#pragma once

#include "tradecore/orderbook/order_book.hpp"
#include <cstdint>

namespace tradecore::matching {

class LiquidityAnalyzer {
public:
    template <std::size_t MaxOrders, std::size_t MaxLevels>
    static bool has_liquidity(const orderbook::OrderBook<MaxOrders, MaxLevels>& book, 
                              uint8_t incoming_side, uint32_t qty) noexcept {
        const auto& opposite_side = (incoming_side == 0) ? book.asks() : book.bids();
        
        uint32_t remaining = qty;
        uint32_t root = opposite_side.avl_root();
        
        if (root == orderbook::kInvalidPoolIndex) {
            return false;
        }

        // To determine if there's enough liquidity, we just scan from BBO until remaining is met.
        // We can just iterate the BBO cache logic (in-order traversal conceptually).
        // Since BBO is cached, we could just traverse next pointers if we maintained a level list,
        // but OrderBook currently only caches BBO, not next level. 
        // We'll traverse the AVL tree in order.
        
        uint32_t found = scan_tree(root, opposite_side.side(), book.level_pool(), remaining);
        return found >= qty;
    }

private:
    template <std::size_t MaxLevels>
    static uint32_t scan_tree(uint32_t node_idx, uint8_t side, 
                              const orderbook::PriceLevelPool<MaxLevels>& level_pool, 
                              uint32_t& remaining) noexcept {
        if (node_idx == orderbook::kInvalidPoolIndex || remaining == 0) return 0;
        
        const auto* node = level_pool.at(node_idx);
        uint32_t found = 0;

        // Bids (descending, right-to-left)
        if (side == 0) {
            found += scan_tree(node->node.right, side, level_pool, remaining);
            if (remaining == 0) return found;

            uint32_t take = std::min(node->total_qty, remaining);
            found += take;
            remaining -= take;
            if (remaining == 0) return found;

            found += scan_tree(node->node.left, side, level_pool, remaining);
        } else {
        // Asks (ascending, left-to-right)
            found += scan_tree(node->node.left, side, level_pool, remaining);
            if (remaining == 0) return found;

            uint32_t take = std::min(node->total_qty, remaining);
            found += take;
            remaining -= take;
            if (remaining == 0) return found;

            found += scan_tree(node->node.right, side, level_pool, remaining);
        }

        return found;
    }
};

} // namespace tradecore::matching
