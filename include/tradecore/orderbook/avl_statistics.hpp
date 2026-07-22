#pragma once

#include <cstdint>

namespace tradecore::orderbook {

// Collected during book operations and replay.
// Reveals tree characteristics and validates hybrid Array+AVL assumptions.
struct AVLStatistics {
    uint64_t ll_rotations{0};   // Right rotations (fixing left-left imbalance)
    uint64_t rr_rotations{0};   // Left rotations (fixing right-right imbalance)
    uint64_t lr_rotations{0};   // Left-right double rotations
    uint64_t rl_rotations{0};   // Right-left double rotations
    uint64_t insertions{0};     // Total level insertions
    uint64_t deletions{0};      // Total level deletions
    uint32_t tree_height{0};    // Updated after every mutation
    double   average_depth{0.0};// Weighted average depth of all nodes

    void reset() noexcept {
        ll_rotations = rr_rotations = lr_rotations = rl_rotations = 0;
        insertions = deletions = 0;
        tree_height = 0;
        average_depth = 0.0;
    }

    [[nodiscard]] uint64_t total_rotations() const noexcept {
        return ll_rotations + rr_rotations + lr_rotations + rl_rotations;
    }
};

} // namespace tradecore::orderbook
