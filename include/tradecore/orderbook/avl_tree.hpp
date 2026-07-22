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
    if (idx == kInvalidPoolIndex) return 0;
    const auto* node = pool.at(idx);
    return 1 + std::max(height(pool, node->left_child),
                        height(pool, node->right_child));
}

template <std::size_t N>
static void update_balance(PriceLevelPool<N>& pool, uint32_t idx) noexcept {
    auto* node = pool.at(idx);
    int32_t lh = height(pool, node->left_child);
    int32_t rh = height(pool, node->right_child);
    node->balance = static_cast<int8_t>(rh - lh);
}

// Rotate right: z is the unbalanced node (balance == -2, left-heavy)
//         z                 y
//        / \               / \
//       y   T4    =>      x   z
//      / \                   / \
//     x   T3                T3  T4
template <std::size_t N>
static uint32_t rotate_right(PriceLevelPool<N>& pool, uint32_t z_idx) noexcept {
    auto* z      = pool.at(z_idx);
    uint32_t y_idx = z->left_child;
    auto* y      = pool.at(y_idx);

    z->left_child = y->right_child;
    if (y->right_child != kInvalidPoolIndex)
        pool.at(y->right_child)->parent = z_idx;

    y->right_child = z_idx;
    y->parent      = z->parent;
    z->parent      = y_idx;

    update_balance(pool, z_idx);
    update_balance(pool, y_idx);
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
    auto* z        = pool.at(z_idx);
    uint32_t y_idx = z->right_child;
    auto* y        = pool.at(y_idx);

    z->right_child = y->left_child;
    if (y->left_child != kInvalidPoolIndex)
        pool.at(y->left_child)->parent = z_idx;

    y->left_child = z_idx;
    y->parent     = z->parent;
    z->parent     = y_idx;

    update_balance(pool, z_idx);
    update_balance(pool, y_idx);
    return y_idx;
}

// Rebalance node at idx if needed. Returns new subtree root.
template <std::size_t N>
static uint32_t rebalance(PriceLevelPool<N>& pool, uint32_t idx,
                          AVLStatistics& stats) noexcept {
    update_balance(pool, idx);
    int8_t bal = pool.at(idx)->balance;

    if (bal < -1) {
        // Left-heavy
        uint32_t left_idx = pool.at(idx)->left_child;
        if (pool.at(left_idx)->balance <= 0) {
            // LL: single right rotation
            ++stats.ll_rotations;
            return rotate_right(pool, idx);
        } else {
            // LR: left rotation on left child, then right rotation on idx
            ++stats.lr_rotations;
            pool.at(idx)->left_child = rotate_left(pool, left_idx);
            if (pool.at(idx)->left_child != kInvalidPoolIndex)
                pool.at(pool.at(idx)->left_child)->parent = idx;
            return rotate_right(pool, idx);
        }
    }
    if (bal > 1) {
        // Right-heavy
        uint32_t right_idx = pool.at(idx)->right_child;
        if (pool.at(right_idx)->balance >= 0) {
            // RR: single left rotation
            ++stats.rr_rotations;
            return rotate_left(pool, idx);
        } else {
            // RL: right rotation on right child, then left rotation on idx
            ++stats.rl_rotations;
            pool.at(idx)->right_child = rotate_right(pool, right_idx);
            if (pool.at(idx)->right_child != kInvalidPoolIndex)
                pool.at(pool.at(idx)->right_child)->parent = idx;
            return rotate_left(pool, idx);
        }
    }
    return idx; // Already balanced
}

// ---- Public API -------------------------------------------------------

// Find minimum (leftmost) node — O(log N)
template <std::size_t N>
uint32_t find_min(const PriceLevelPool<N>& pool, uint32_t root_idx) noexcept {
    if (root_idx == kInvalidPoolIndex) return kInvalidPoolIndex;
    uint32_t curr = root_idx;
    while (pool.at(curr)->left_child != kInvalidPoolIndex)
        curr = pool.at(curr)->left_child;
    return curr;
}

// Find maximum (rightmost) node — O(log N)
template <std::size_t N>
uint32_t find_max(const PriceLevelPool<N>& pool, uint32_t root_idx) noexcept {
    if (root_idx == kInvalidPoolIndex) return kInvalidPoolIndex;
    uint32_t curr = root_idx;
    while (pool.at(curr)->right_child != kInvalidPoolIndex)
        curr = pool.at(curr)->right_child;
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
        curr = (price < node->price) ? node->left_child : node->right_child;
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
        auto* node       = pool.at(new_idx);
        node->left_child = kInvalidPoolIndex;
        node->right_child= kInvalidPoolIndex;
        node->parent     = kInvalidPoolIndex;
        node->balance    = 0;
        return new_idx;
    }

    auto* root  = pool.at(root_idx);
    auto* node  = pool.at(new_idx);

    if (node->price < root->price) {
        root->left_child = insert(pool, root->left_child, new_idx, stats);
        pool.at(root->left_child)->parent = root_idx;
    } else {
        root->right_child = insert(pool, root->right_child, new_idx, stats);
        pool.at(root->right_child)->parent = root_idx;
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
        root->left_child = remove(pool, root->left_child, target_idx, stats);
        if (root->left_child != kInvalidPoolIndex)
            pool.at(root->left_child)->parent = root_idx;
        return rebalance(pool, root_idx, stats);
    }
    if (target->price > root->price) {
        root->right_child = remove(pool, root->right_child, target_idx, stats);
        if (root->right_child != kInvalidPoolIndex)
            pool.at(root->right_child)->parent = root_idx;
        return rebalance(pool, root_idx, stats);
    }

    // root_idx == target_idx: perform removal
    TRADECORE_ASSERT(root_idx == target_idx);

    if (root->left_child == kInvalidPoolIndex) {
        // 0 or 1 child (right)
        uint32_t right = root->right_child;
        if (right != kInvalidPoolIndex)
            pool.at(right)->parent = root->parent;
        root->left_child = root->right_child = kInvalidPoolIndex;
        return right;
    }
    if (root->right_child == kInvalidPoolIndex) {
        // 1 child (left)
        uint32_t left = root->left_child;
        pool.at(left)->parent = root->parent;
        root->left_child = root->right_child = kInvalidPoolIndex;
        return left;
    }

    // 2 children: find in-order successor (leftmost of right subtree),
    // splice it out and put it in place of the deleted node.
    uint32_t succ_idx = find_min(pool, root->right_child);
    --stats.deletions; // successor removal is part of this operation, not a new one

    uint32_t new_right = remove(pool, root->right_child, succ_idx, stats);

    auto* succ        = pool.at(succ_idx);
    uint32_t saved_parent = root->parent;

    succ->left_child  = root->left_child;
    succ->right_child = new_right;
    succ->parent      = saved_parent;

    if (root->left_child != kInvalidPoolIndex)
        pool.at(root->left_child)->parent = succ_idx;
    if (new_right != kInvalidPoolIndex)
        pool.at(new_right)->parent = succ_idx;

    // Clear deleted node's tree links
    root->left_child = root->right_child = kInvalidPoolIndex;

    return rebalance(pool, succ_idx, stats);
}

// Update tree_height stat after a mutation
template <std::size_t N>
void update_stats_height(const PriceLevelPool<N>& pool, uint32_t root_idx,
                         AVLStatistics& stats) noexcept {
    stats.tree_height = static_cast<uint32_t>(height(pool, root_idx));
}

} // namespace tradecore::orderbook::avl
