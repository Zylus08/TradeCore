#pragma once

#include "tradecore/orderbook/order.hpp"
#include "tradecore/orderbook/price_level.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <span>
#include <cstdint>
#include <string>
#include <unordered_set>
#include "tradecore/orderbook/flat_hash_map.hpp"

namespace tradecore::orderbook {

// Result of a verification pass
struct VerificationResult {
    bool ok{true};
    std::string first_failure{};

    void fail(const char* msg) {
        if (ok) {
            ok = false;
            first_failure = msg;
        }
    }
};

// OrderBookVerifier:
// A zero-cost in-production, full-correctness-in-debug invariant framework.
// Lives entirely in the orderbook subsystem. Receives raw view of pool arrays.
// Compiled out entirely when NDEBUG is defined.
class OrderBookVerifier {
public:
    // Primary entry point called after every book mutation in debug builds.
    static VerificationResult verify(
        std::span<const PriceLevel> bid_levels,  // Contiguous array of bid PriceLevels
        std::span<const PriceLevel> ask_levels,  // Contiguous array of ask PriceLevels
        std::span<const Order>      order_pool,  // The full object pool
        const FlatHashMap&          order_map,   // OrderId -> pool index
        uint32_t bid_root_idx,                   // Root of AVL bid tree (pool index)
        uint32_t ask_root_idx,                   // Root of AVL ask tree (pool index)
        uint32_t best_bid_idx,                   // Pool index of BBO bid level
        uint32_t best_ask_idx                    // Pool index of BBO ask level
    ) {
        VerificationResult result;

#ifndef NDEBUG
        // 1. Bids sorted descending
        check_level_ordering(bid_levels, false, result);
        if (!result.ok) return result;

        // 2. Asks sorted ascending
        check_level_ordering(ask_levels, true, result);
        if (!result.ok) return result;

        // 3. No crossed book: best bid < best ask
        check_no_crossed_book(bid_levels, ask_levels, best_bid_idx, best_ask_idx, result);
        if (!result.ok) return result;

        // 4. No duplicate order IDs + every order belongs to exactly one level
        check_order_uniqueness(bid_levels, ask_levels, order_pool, order_map, result);
        if (!result.ok) return result;

        // 5. Price level quantity == sum(order.remaining_qty)
        check_level_quantities(bid_levels, order_pool, result);
        check_level_quantities(ask_levels, order_pool, result);
        if (!result.ok) return result;

        // 6. Hash map pointer validity
        check_order_map_consistency(bid_levels, ask_levels, order_pool, order_map, result);
        if (!result.ok) return result;

        // 7. AVL topology valid (height matches computed, parent pointers correct, cycle-free)
        std::unordered_set<uint32_t> visited;
        check_avl_topology(bid_levels, bid_root_idx, kInvalidPoolIndex, visited, result);
        visited.clear();
        check_avl_topology(ask_levels, ask_root_idx, kInvalidPoolIndex, visited, result);
        if (!result.ok) return result;

        // 8. FIFO ordering (timestamp of head <= timestamp of tail for every level)
        check_fifo_ordering(bid_levels, order_pool, result);
        check_fifo_ordering(ask_levels, order_pool, result);
        if (!result.ok) return result;

        // 9. BBO Cache Verification
        check_bbo_cache(bid_levels, bid_root_idx, best_bid_idx, false, result);
        check_bbo_cache(ask_levels, ask_root_idx, best_ask_idx, true, result);
#endif

        return result;
    }

private:
#ifndef NDEBUG
    static void check_level_ordering(
        std::span<const PriceLevel> levels, bool ascending, VerificationResult& result)
    {
        if (levels.size() <= 1) return;
        for (std::size_t i = 1; i < levels.size(); ++i) {
            const auto& prev = levels[i - 1];
            const auto& curr = levels[i];
            // Only check active levels (non-zero price)
            if (prev.price == 0 || curr.price == 0) continue;
            if (ascending) {
                if (prev.price >= curr.price) {
                    result.fail("Ask levels not strictly ascending");
                    return;
                }
            } else {
                if (prev.price <= curr.price) {
                    result.fail("Bid levels not strictly descending");
                    return;
                }
            }
        }
    }

    static void check_no_crossed_book(
        std::span<const PriceLevel> bid_levels,
        std::span<const PriceLevel> ask_levels,
        uint32_t best_bid_idx,
        uint32_t best_ask_idx,
        VerificationResult& result)
    {
        if (best_bid_idx == kInvalidPoolIndex || best_ask_idx == kInvalidPoolIndex) return;
        if (best_bid_idx >= bid_levels.size() || best_ask_idx >= ask_levels.size()) return;
        if (bid_levels[best_bid_idx].price >= ask_levels[best_ask_idx].price) {
            result.fail("Crossed book: best_bid.price >= best_ask.price");
        }
    }

    static void check_order_uniqueness(
        std::span<const PriceLevel> bid_levels,
        std::span<const PriceLevel> ask_levels,
        std::span<const Order> order_pool,
        const FlatHashMap& order_map,
        VerificationResult& result)
    {
        std::unordered_set<uint64_t> seen_orders;
        
        // Lambda to check level orders
        auto check_levels = [&](std::span<const PriceLevel> levels) {
            for (const auto& level : levels) {
                if (level.order_count == 0) continue;
                uint32_t curr = level.head_order_idx;
                while (curr != kInvalidPoolIndex) {
                    if (curr >= order_pool.size()) {
                        result.fail("Order pool index out of bounds");
                        return;
                    }
                    uint64_t oid = order_pool[curr].order_id;
                    if (!seen_orders.insert(oid).second) {
                        result.fail("Duplicate OrderId found within levels");
                        return;
                    }
                    // Must be in map
                    if (!order_map.contains(oid)) {
                        result.fail("Order present in level but missing from map (ghost order)");
                        return;
                    }
                    curr = order_pool[curr].next_in_level;
                }
            }
        };
        walk(bid_levels);
        walk(ask_levels);

        if (!result.ok) return;

        // Pass 2: Every pool slot referenced by the hash map must appear
        //         in exactly one level (caught above) AND in at least one
        //         level. Slots in the map but absent from all level lists
        //         are orphaned orders — a leaked reference.
        for (const auto& [id, pool_idx] : order_map) {
            if (seen_pool_indices.find(pool_idx) == seen_pool_indices.end()) {
                result.fail("Order in hash map belongs to no price level (orphaned)");
                return;
            }
        }

        // Pass 3: Every pool index reached by walking levels must be in
        //         the hash map. If not, the level's list contains a ghost
        //         entry that the book has already lost track of.
        for (uint32_t pool_idx : seen_pool_indices) {
            const uint64_t oid = order_pool[pool_idx].order_id;
            if (order_map.find(oid) == order_map.end()) {
                result.fail("Order reachable from price level is absent from hash map (ghost)");
                return;
            }
        }
    }

    static void check_level_quantities(
        std::span<const PriceLevel> levels,
        std::span<const Order> order_pool,
        VerificationResult& result)
    {
        for (const auto& level : levels) {
            if (level.order_count == 0) continue;
            uint32_t sum = 0;
            uint32_t idx = level.head_order_idx;
            while (idx != kInvalidPoolIndex) {
                if (idx >= order_pool.size()) {
                    result.fail("Pool index OOB in level qty check");
                    return;
                }
                sum += order_pool[idx].remaining_qty;
                idx = order_pool[idx].next_in_level;
            }
            if (sum != level.total_qty) {
                result.fail("Level total_qty != sum(order.remaining_qty)");
            }
        }
    }

    static void check_order_map_consistency(
        std::span<const PriceLevel> bid_levels,
        std::span<const PriceLevel> ask_levels,
        std::span<const Order> order_pool,
        const std::unordered_map<uint64_t, uint32_t>& order_map,
        VerificationResult& result)
    {
        // Every entry in the map must point to an active order in a real level
        for (const auto& [id, pool_idx] : order_map) {
            if (pool_idx >= order_pool.size()) {
                result.fail("HashMap points to invalid pool index");
                return;
            }
            if (order_pool[pool_idx].order_id != id) {
                result.fail("HashMap order_id mismatch with pool slot");
                return;
            }
        }
    }

    static int check_avl_topology(
        std::span<const PriceLevel> levels,
        uint32_t idx,
        uint32_t expected_parent,
        std::unordered_set<uint32_t>& visited,
        VerificationResult& result)
    {
        if (idx == kInvalidPoolIndex) return 0;
        if (idx >= levels.size()) {
            result.fail("AVL child index out of bounds");
            return -1;
        }
        if (!visited.insert(idx).second) {
            result.fail("AVL tree contains cycles (node reachable multiple times)");
            return -1;
        }
        const auto& node = levels[idx].node;

        if (node.parent != expected_parent) {
            result.fail("AVL parent pointer mismatch");
            return -1;
        }

        int left_h  = check_avl_topology(levels, node.left, idx, visited, result);
        int right_h = check_avl_topology(levels, node.right, idx, visited, result);

        if (!result.ok) return -1;

        int computed_height = 1 + std::max(left_h, right_h);
        if (node.height != computed_height) {
            result.fail("AVL stored height does not match computed height");
        }

        int balance = right_h - left_h;
        if (balance < -1 || balance > 1) {
            result.fail("AVL balance factor out of range [-1, 1]");
        }
        return computed_height;
    }

    static void check_bbo_cache(
        std::span<const PriceLevel> levels,
        uint32_t root_idx,
        uint32_t bbo_idx,
        bool is_ask,
        VerificationResult& result)
    {
        if (root_idx == kInvalidPoolIndex) {
            if (bbo_idx != kInvalidPoolIndex) result.fail("BBO cache is set but tree is empty");
            return;
        }
        if (bbo_idx == kInvalidPoolIndex) {
            result.fail("BBO cache is invalid but tree is not empty");
            return;
        }

        uint32_t curr = root_idx;
        if (is_ask) {
            while (levels[curr].node.left != kInvalidPoolIndex) curr = levels[curr].node.left;
        } else {
            while (levels[curr].node.right != kInvalidPoolIndex) curr = levels[curr].node.right;
        }

        if (curr != bbo_idx) {
            result.fail("BBO cache does not match tree min/max");
        }
    }

    static void check_fifo_ordering(
        std::span<const PriceLevel> levels,
        std::span<const Order> order_pool,
        VerificationResult& result)
    {
        for (const auto& level : levels) {
            if (level.order_count <= 1) continue;
            uint32_t idx = level.head_order_idx;
            uint64_t prev_ts = 0;
            while (idx != kInvalidPoolIndex) {
                if (idx >= order_pool.size()) {
                    result.fail("Pool index OOB in FIFO check");
                    return;
                }
                if (order_pool[idx].timestamp_ns < prev_ts) {
                    result.fail("FIFO violated: timestamp not monotonic within price level");
                    return;
                }
                prev_ts = order_pool[idx].timestamp_ns;
                idx = order_pool[idx].next_in_level;
            }
        }
    }
#endif // NDEBUG
};

// Convenience macro: does nothing in Release builds
#ifndef NDEBUG
#define TRADECORE_VERIFY_BOOK(bid_levels, ask_levels, pool, map, bid_root, ask_root, bb, ba) \
    do { \
        auto _vr = tradecore::orderbook::OrderBookVerifier::verify( \
            bid_levels, ask_levels, pool, map, bid_root, ask_root, bb, ba); \
        TRADECORE_VERIFY(_vr.ok); \
    } while (0)
#else
#define TRADECORE_VERIFY_BOOK(...) do {} while (0)
#endif

} // namespace tradecore::orderbook
