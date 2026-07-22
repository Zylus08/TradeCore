#include <gtest/gtest.h>
#include "tradecore/orderbook/avl_tree.hpp"
#include "tradecore/orderbook/book_side.hpp"
#include "tradecore/orderbook/price_level_pool.hpp"
#include "tradecore/orderbook/avl_statistics.hpp"
#include <algorithm>
#include <random>
#include <vector>
#include <set>
#include <cmath>

using namespace tradecore::orderbook;
namespace avl = tradecore::orderbook::avl;

// ---- Helpers ---------------------------------------------------------------

static constexpr std::size_t kPoolSize = 512;
using Pool = PriceLevelPool<kPoolSize>;
using Side = BookSide<kPoolSize>;

// Inserts price levels into BookSide and returns a vector of their pool indices
static std::vector<uint32_t> insert_prices(Side& side, Pool& pool,
                                            AVLStatistics& stats,
                                            std::initializer_list<uint32_t> prices)
{
    std::vector<uint32_t> indices;
    for (auto p : prices) {
        uint32_t idx = side.create_level(p, pool);
        side.insert_level(idx, pool, stats);
        indices.push_back(idx);
    }
    return indices;
}

// Verify AVL invariant recursively
static bool verify_avl(const Pool& pool, uint32_t idx, uint32_t min, uint32_t max) {
    if (idx == kInvalidPoolIndex) return true;
    const auto* node = pool.at(idx);
    if (node->price <= min || node->price >= max) return false;
    int8_t expected_bal = static_cast<int8_t>(
        avl::height(pool, node->right_child) - avl::height(pool, node->left_child));
    if (node->balance != expected_bal) return false;
    if (node->balance < -1 || node->balance > 1) return false;
    return verify_avl(pool, node->left_child, min, node->price) &&
           verify_avl(pool, node->right_child, node->price, max);
}

static bool verify_avl(const Pool& pool, uint32_t root) {
    return verify_avl(pool, root, 0, UINT32_MAX);
}

// ============================================================================
// Step 2: AVL Rotations — each triggered deterministically
// ============================================================================

// LL imbalance: insert 3,2,1 => right rotation at 3
TEST(AVLTreeTest, LLRotation) {
    Pool pool;
    Side side(0); // bid
    AVLStatistics stats;

    insert_prices(side, pool, stats, {300, 200, 100});

    EXPECT_EQ(stats.ll_rotations, 1U);
    EXPECT_EQ(stats.rr_rotations, 0U);
    EXPECT_EQ(stats.lr_rotations, 0U);
    EXPECT_EQ(stats.rl_rotations, 0U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_EQ(pool.at(side.avl_root())->price, 200U);
}

// RR imbalance: insert 1,2,3 => left rotation at 1
TEST(AVLTreeTest, RRRotation) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    insert_prices(side, pool, stats, {100, 200, 300});

    EXPECT_EQ(stats.rr_rotations, 1U);
    EXPECT_EQ(stats.ll_rotations, 0U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_EQ(pool.at(side.avl_root())->price, 200U);
}

// LR imbalance: insert 3,1,2 => left rotation at 1, then right rotation at 3
TEST(AVLTreeTest, LRRotation) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    insert_prices(side, pool, stats, {300, 100, 200});

    EXPECT_EQ(stats.lr_rotations, 1U);
    EXPECT_EQ(stats.ll_rotations, 0U);
    EXPECT_EQ(stats.rr_rotations, 0U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_EQ(pool.at(side.avl_root())->price, 200U);
}

// RL imbalance: insert 1,3,2 => right rotation at 3, then left rotation at 1
TEST(AVLTreeTest, RLRotation) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    insert_prices(side, pool, stats, {100, 300, 200});

    EXPECT_EQ(stats.rl_rotations, 1U);
    EXPECT_EQ(stats.rr_rotations, 0U);
    EXPECT_EQ(stats.ll_rotations, 0U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_EQ(pool.at(side.avl_root())->price, 200U);
}

// ============================================================================
// Step 3: AVL Deletion
// ============================================================================

// Leaf deletion
TEST(AVLTreeTest, DeleteLeaf) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    auto ids = insert_prices(side, pool, stats, {200, 100, 300});
    // Tree:   200
    //        /   \
    //      100   300
    // Delete leaf 100
    side.remove_and_destroy_level(ids[1], pool, stats);

    EXPECT_EQ(side.level_count(), 2U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_EQ(avl::find(pool, side.avl_root(), 100U), kInvalidPoolIndex);
    EXPECT_NE(avl::find(pool, side.avl_root(), 200U), kInvalidPoolIndex);
    EXPECT_NE(avl::find(pool, side.avl_root(), 300U), kInvalidPoolIndex);
}

// One-child deletion
TEST(AVLTreeTest, DeleteOneChild) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    insert_prices(side, pool, stats, {200, 100, 300, 250});
    // Tree:     200
    //          /   \
    //        100   300
    //              /
    //            250
    uint32_t idx_300 = avl::find(pool, side.avl_root(), 300U);
    ASSERT_NE(idx_300, kInvalidPoolIndex);

    side.remove_and_destroy_level(idx_300, pool, stats);

    EXPECT_EQ(side.level_count(), 3U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_NE(avl::find(pool, side.avl_root(), 250U), kInvalidPoolIndex);
    EXPECT_EQ(avl::find(pool, side.avl_root(), 300U), kInvalidPoolIndex);
}

// Two-children deletion
TEST(AVLTreeTest, DeleteTwoChildren) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    insert_prices(side, pool, stats, {200, 100, 300, 50, 150, 250, 350});
    // Full tree. Delete root (200) which has two children.
    uint32_t root_before = side.avl_root();
    side.remove_and_destroy_level(root_before, pool, stats);

    EXPECT_EQ(side.level_count(), 6U);
    EXPECT_TRUE(verify_avl(pool, side.avl_root()));
    EXPECT_EQ(avl::find(pool, side.avl_root(), 200U), kInvalidPoolIndex);
}

// Root deletion (single node)
TEST(AVLTreeTest, DeleteRoot) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    auto ids = insert_prices(side, pool, stats, {100});
    side.remove_and_destroy_level(ids[0], pool, stats);

    EXPECT_EQ(side.level_count(), 0U);
    EXPECT_EQ(side.avl_root(), kInvalidPoolIndex);
    EXPECT_FALSE(side.has_bbo());
}

// ============================================================================
// Step 4: BBO Cache
// ============================================================================

TEST(BookSideTest, BBOBidUpdatesOnInsert) {
    Pool pool;
    Side side(0); // bid: best = max price
    AVLStatistics stats;

    EXPECT_FALSE(side.has_bbo());

    uint32_t i1 = side.create_level(100, pool); side.insert_level(i1, pool, stats);
    EXPECT_TRUE(side.has_bbo());
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 100U);

    uint32_t i2 = side.create_level(200, pool); side.insert_level(i2, pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 200U); // higher price wins

    uint32_t i3 = side.create_level(150, pool); side.insert_level(i3, pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 200U); // 200 still best
}

TEST(BookSideTest, BBOAskUpdatesOnInsert) {
    Pool pool;
    Side side(1); // ask: best = min price
    AVLStatistics stats;

    uint32_t i1 = side.create_level(300, pool); side.insert_level(i1, pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 300U);

    uint32_t i2 = side.create_level(200, pool); side.insert_level(i2, pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 200U); // lower price wins

    uint32_t i3 = side.create_level(250, pool); side.insert_level(i3, pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 200U); // 200 still best
}

TEST(BookSideTest, BBORecoveryAfterBBORemoval) {
    Pool pool;
    Side side(0); // bid
    AVLStatistics stats;

    auto ids = insert_prices(side, pool, stats, {100, 200, 300});
    // BBO should be 300
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 300U);

    // Remove best bid (300) — BBO must recover to 200
    side.remove_and_destroy_level(ids[2], pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 200U);

    // Remove 200 — BBO must recover to 100
    side.remove_and_destroy_level(ids[0], pool, stats);
    EXPECT_EQ(pool.at(side.bbo_idx())->price, 200U);
}

// ============================================================================
// Step 5: Level Lifecycle (pool recycling)
// ============================================================================

TEST(BookSideTest, PoolSlotRecycled) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    EXPECT_EQ(pool.active_count(), 0U);
    uint32_t idx = side.create_level(100, pool);
    EXPECT_EQ(pool.active_count(), 1U);
    side.insert_level(idx, pool, stats);
    side.remove_and_destroy_level(idx, pool, stats);
    EXPECT_EQ(pool.active_count(), 0U);
}

// ============================================================================
// Stress Test: 10,000 random insert/delete — tree must stay valid
// ============================================================================

TEST(AVLTreeTest, StressRandomInsertDelete) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> price_dist(1, 99999);

    std::set<uint32_t> active_prices;
    std::vector<std::pair<uint32_t, uint32_t>> active_levels; // (price, pool_idx)

    for (int op = 0; op < 10000; ++op) {
        bool do_insert = active_levels.empty()
                      || (active_levels.size() < 200 && rng() % 2 == 0);
        if (do_insert) {
            uint32_t price;
            do { price = price_dist(rng); } while (active_prices.count(price));
            active_prices.insert(price);
            uint32_t idx = side.create_level(price, pool);
            ASSERT_NE(idx, kInvalidPoolIndex) << "Pool exhausted at op " << op;
            side.insert_level(idx, pool, stats);
            active_levels.push_back({price, idx});
        } else {
            // Delete a random existing level
            std::uniform_int_distribution<std::size_t> pick(0, active_levels.size() - 1);
            std::size_t i = pick(rng);
            auto [price, idx] = active_levels[i];
            active_levels.erase(active_levels.begin() + static_cast<std::ptrdiff_t>(i));
            active_prices.erase(price);
            side.remove_and_destroy_level(idx, pool, stats);
        }

        // AVL invariant must hold after every single operation
        ASSERT_TRUE(verify_avl(pool, side.avl_root()))
            << "AVL invariant violated at op " << op;

        // Tree height must be within AVL bound
        if (side.level_count() > 1) {
            double max_height = 1.45 * std::log2(static_cast<double>(side.level_count()) + 1.0);
            ASSERT_LE(stats.tree_height, static_cast<uint32_t>(max_height) + 2)
                << "AVL height exceeded bound at op " << op
                << " (n=" << side.level_count()
                << ", h=" << stats.tree_height << ")";
        }
    }
}

// ============================================================================
// Statistics sanity
// ============================================================================

TEST(AVLStatisticsTest, TotalRotations) {
    Pool pool;
    Side side(0);
    AVLStatistics stats;

    insert_prices(side, pool, stats, {300, 200, 100}); // LL
    insert_prices(side, pool, stats, {400, 500});       // RR
    EXPECT_EQ(stats.total_rotations(), stats.ll_rotations + stats.rr_rotations +
                                       stats.lr_rotations + stats.rl_rotations);
}
