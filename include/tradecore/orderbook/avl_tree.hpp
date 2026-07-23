#pragma once

#include "tradecore/orderbook/price_level.hpp"
#include "tradecore/orderbook/price_level_pool.hpp"
#include "tradecore/orderbook/avl_statistics.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <algorithm>
#include <cstdint>

// AVL tree operating on pool-indexed PriceLevel nodes.
// Standard BST ordering: left.price < node.price < right.price.
//   - Best Bid = rightmost node (max price)
//   - Best Ask = leftmost node  (min price)
// All functions are free templates; pool passed by reference.

namespace tradecore::orderbook::avl {

// ---- Internal helpers ------------------------------------------------

template <std::size_t N>
static int32_t height(const PriceLevelPool<N>& pool, uint32_t idx) noexcept {
    return (idx == kInvalidPoolIndex) ? 0 : pool.at(idx)->node.height;
}

template <std::size_t N>
static int32_t balance_factor(const PriceLevelPool<N>& pool, uint32_t idx) noexcept {
    const auto& n = pool.at(idx)->node;
    return height(pool, n.right) - height(pool, n.left);
}

template <std::size_t N>
static void update_height(PriceLevelPool<N>& pool, uint32_t idx) noexcept {
    auto& n = pool.at(idx)->node;
    n.height = 1 + std::max(height(pool, n.left), height(pool, n.right));
}

// Rotate right: z is the unbalanced node (balance == -2, left-heavy)
//         z                 y
//        / \               / \
//       y   T4    =>      x   z
//      / \                   / \
//     x   T3                T3  T4
template <std::size_t N>
static uint32_t rotate_right(PriceLevelPool<N>& pool, uint32_t z_idx) noexcept {
    auto& z = pool.at(z_idx)->node;
    uint32_t y_idx = z.left;
    auto& y = pool.at(y_idx)->node;

    z.left = y.right;
    if (y.right != kInvalidPoolIndex)
        pool.at(y.right)->node.parent = z_idx;

    y.right  = z_idx;
    y.parent = z.parent;
    z.parent = y_idx;

    update_height(pool, z_idx);
    update_height(pool, y_idx);
    return y_idx;
}

// Rotate left: z is the unbalanced node (balance == +2, right-heavy)
//      z                   y
//     / \                 / \
//    T1   y     =>       z   x
//        / \            / \
//       T2   x         T1  T2
template <std::size_t N>
static uint32_t rotate_left(PriceLevelPool<N>& pool, uint32_t z_idx) noexcept {
    auto& z = pool.at(z_idx)->node;
    uint32_t y_idx = z.right;
    auto& y = pool.at(y_idx)->node;

    z.right = y.left;
    if (y.left != kInvalidPoolIndex)
        pool.at(y.left)->node.parent = z_idx;

    y.left   = z_idx;
    y.parent = z.parent;
    z.parent = y_idx;

    update_height(pool, z_idx);
    update_height(pool, y_idx);
    return y_idx;
}

// ---- Public API -------------------------------------------------------

// Rebalance node at idx if needed. Returns new subtree root.
template <std::size_t N>
uint32_t rebalance(PriceLevelPool<N>& pool, uint32_t idx,
                   AVLStatistics& stats) noexcept {
    update_height(pool, idx);
    int32_t bal = balance_factor(pool, idx);

    if (bal < -1) {
        // Left-heavy
        uint32_t left_idx = pool.at(idx)->node.left;
        if (balance_factor(pool, left_idx) <= 0) {
            // LL: single right rotation
            ++stats.ll_rotations;
            return rotate_right(pool, idx);
        } else {
            // LR: left rotation on left child, then right rotation on idx
            ++stats.lr_rotations;
            pool.at(idx)->node.left = rotate_left(pool, left_idx);
            if (pool.at(idx)->node.left != kInvalidPoolIndex)
                pool.at(pool.at(idx)->node.left)->node.parent = idx;
            return rotate_right(pool, idx);
        }
    }
    if (bal > 1) {
        // Right-heavy
        uint32_t right_idx = pool.at(idx)->node.right;
        if (balance_factor(pool, right_idx) >= 0) {
            // RR: single left rotation
            ++stats.rr_rotations;
            return rotate_left(pool, idx);
        } else {
            // RL: right rotation on right child, then left rotation on idx
            ++stats.rl_rotations;
            pool.at(idx)->node.right = rotate_right(pool, right_idx);
            if (pool.at(idx)->node.right != kInvalidPoolIndex)
                pool.at(pool.at(idx)->node.right)->node.parent = idx;
            return rotate_left(pool, idx);
        }
    }
    return idx; // Already balanced
}

// Find minimum (leftmost) node — O(log N)
template <std::size_t N>
uint32_t find_min(const PriceLevelPool<N>& pool, uint32_t root_idx) noexcept {
    if (root_idx == kInvalidPoolIndex) return kInvalidPoolIndex;
    uint32_t curr = root_idx;
    while (pool.at(curr)->node.left != kInvalidPoolIndex)
        curr = pool.at(curr)->node.left;
    return curr;
}

// Find maximum (rightmost) node — O(log N)
template <std::size_t N>
uint32_t find_max(const PriceLevelPool<N>& pool, uint32_t root_idx) noexcept {
    if (root_idx == kInvalidPoolIndex) return kInvalidPoolIndex;
    uint32_t curr = root_idx;
    while (pool.at(curr)->node.right != kInvalidPoolIndex)
        curr = pool.at(curr)->node.right;
    return curr;
}

// Find level by exact price — O(log N)
template <std::size_t N>
uint32_t find(const PriceLevelPool<N>& pool, uint32_t root_idx,
              uint32_t price) noexcept {
    uint32_t curr = root_idx;
    while (curr != kInvalidPoolIndex) {
        const auto* node = pool.at(curr);
        if (price == node->price) return curr;
        curr = (price < node->price) ? node->node.left : node->node.right;
    }
    return kInvalidPoolIndex;
}

// Insert a pre-allocated node (new_idx) into the tree.
// The node's price must be set before calling. Returns new root.
template <std::size_t N>
uint32_t insert(PriceLevelPool<N>& pool, uint32_t root_idx, uint32_t new_idx,
                AVLStatistics& stats) noexcept {
    TRADECORE_ASSERT(new_idx != kInvalidPoolIndex);
    ++stats.insertions;

    if (root_idx == kInvalidPoolIndex) {
        auto& node   = pool.at(new_idx)->node;
        node.left    = kInvalidPoolIndex;
        node.right   = kInvalidPoolIndex;
        node.parent  = kInvalidPoolIndex;
        node.height  = 1;
        return new_idx;
    }

    auto* root  = pool.at(root_idx);
    auto* node  = pool.at(new_idx);

    if (node->price < root->price) {
        root->node.left = insert(pool, root->node.left, new_idx, stats);
        pool.at(root->node.left)->node.parent = root_idx;
    } else {
        root->node.right = insert(pool, root->node.right, new_idx, stats);
        pool.at(root->node.right)->node.parent = root_idx;
    }

    return rebalance(pool, root_idx, stats);
}

// Remove a node by pool index from the tree.
// Returns new root. Caller must free the pool slot afterwards.
template <std::size_t N>
uint32_t remove(PriceLevelPool<N>& pool, uint32_t root_idx, uint32_t target_idx,
                AVLStatistics& stats) noexcept {
    if (root_idx == kInvalidPoolIndex) return kInvalidPoolIndex;
    ++stats.deletions;

    auto* root   = pool.at(root_idx);
    auto* target = pool.at(target_idx);

    if (target->price < root->price) {
        root->node.left = remove(pool, root->node.left, target_idx, stats);
        if (root->node.left != kInvalidPoolIndex)
            pool.at(root->node.left)->node.parent = root_idx;
        return rebalance(pool, root_idx, stats);
    }
    if (target->price > root->price) {
        root->node.right = remove(pool, root->node.right, target_idx, stats);
        if (root->node.right != kInvalidPoolIndex)
            pool.at(root->node.right)->node.parent = root_idx;
        return rebalance(pool, root_idx, stats);
    }

    // root_idx == target_idx: perform removal
    TRADECORE_ASSERT(root_idx == target_idx);

    if (root->node.left == kInvalidPoolIndex) {
        // 0 or 1 child (right)
        uint32_t right = root->node.right;
        if (right != kInvalidPoolIndex)
            pool.at(right)->node.parent = root->node.parent;
        root->node.left = root->node.right = kInvalidPoolIndex;
        return right;
    }
    if (root->node.right == kInvalidPoolIndex) {
        // 1 child (left)
        uint32_t left = root->node.left;
        pool.at(left)->node.parent = root->node.parent;
        root->node.left = root->node.right = kInvalidPoolIndex;
        return left;
    }

    // 2 children: find in-order successor (leftmost of right subtree),
    // splice it out and put it in place of the deleted node.
    uint32_t succ_idx = find_min(pool, root->node.right);
    --stats.deletions; // successor removal is part of this operation

    uint32_t new_right = remove(pool, root->node.right, succ_idx, stats);

    auto& succ        = pool.at(succ_idx)->node;
    uint32_t saved_parent = root->node.parent;

    succ.left   = root->node.left;
    succ.right  = new_right;
    succ.parent = saved_parent;

    if (root->node.left != kInvalidPoolIndex)
        pool.at(root->node.left)->node.parent = succ_idx;
    if (new_right != kInvalidPoolIndex)
        pool.at(new_right)->node.parent = succ_idx;

    // Clear deleted node's tree links
    root->node.left = root->node.right = kInvalidPoolIndex;

    return rebalance(pool, succ_idx, stats);
}

// Update tree_height stat after a mutation
template <std::size_t N>
void update_stats_height(const PriceLevelPool<N>& pool, uint32_t root_idx,
                         AVLStatistics& stats) noexcept {
    stats.tree_height = static_cast<uint32_t>(height(pool, root_idx));
    if (stats.tree_height > stats.maximum_height) {
        stats.maximum_height = stats.tree_height;
    }
}

} // namespace tradecore::orderbook::avl
