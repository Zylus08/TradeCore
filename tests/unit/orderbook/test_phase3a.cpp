#include <gtest/gtest.h>
#include "tradecore/orderbook/order_pool.hpp"
#include "tradecore/orderbook/price_level_pool.hpp"
#include "tradecore/orderbook/flat_hash_map.hpp"
#include "tradecore/orderbook/book_side.hpp"

using namespace tradecore::orderbook;

// =====================================================================
// OrderPool Tests
// =====================================================================
TEST(OrderPoolTest, AllocateDeallocate) {
    OrderPool<16> pool;
    EXPECT_EQ(pool.active_count(), 0);

    Order* o = pool.allocate();
    ASSERT_NE(o, nullptr);
    o->order_id = 999;
    EXPECT_EQ(pool.active_count(), 1);

    uint32_t idx = pool.index_of(o);
    EXPECT_EQ(pool.at(idx)->order_id, 999U);

    pool.deallocate(o);
    EXPECT_EQ(pool.active_count(), 0);
}

TEST(OrderPoolTest, CacheLineAlignment) {
    static_assert(sizeof(Order) == 64,    "Order must be 64 bytes");
    static_assert(alignof(Order) == 64,   "Order must be 64-byte aligned");
}

// =====================================================================
// PriceLevelPool Tests
// =====================================================================
TEST(PriceLevelPoolTest, AllocateDeallocate) {
    PriceLevelPool<8> pool;

    PriceLevel* lvl = pool.allocate();
    ASSERT_NE(lvl, nullptr);
    lvl->price = 10000;
    EXPECT_EQ(pool.active_count(), 1);

    pool.deallocate(lvl);
    EXPECT_EQ(pool.active_count(), 0);
}

TEST(PriceLevelPoolTest, CacheLineAlignment) {
    static_assert(sizeof(PriceLevel) == 64,  "PriceLevel must be 64 bytes");
    static_assert(alignof(PriceLevel) == 64, "PriceLevel must be 64-byte aligned");
}

// =====================================================================
// FlatHashMap Tests
// =====================================================================
TEST(FlatHashMapTest, InsertAndFind) {
    FlatHashMap map(16);
    EXPECT_TRUE(map.insert(1001, 0));
    EXPECT_TRUE(map.insert(1002, 1));
    EXPECT_TRUE(map.insert(1003, 2));

    EXPECT_EQ(map.find(1001), 0U);
    EXPECT_EQ(map.find(1002), 1U);
    EXPECT_EQ(map.find(1003), 2U);
    EXPECT_EQ(map.find(9999), FlatHashMap::kEmpty);
    EXPECT_EQ(map.size(), 3U);
}

TEST(FlatHashMapTest, Erase) {
    FlatHashMap map(16);
    map.insert(42, 7);

    EXPECT_TRUE(map.contains(42));
    EXPECT_TRUE(map.erase(42));
    EXPECT_FALSE(map.contains(42));
    EXPECT_EQ(map.size(), 0U);

    // Erase non-existent
    EXPECT_FALSE(map.erase(42));
}

TEST(FlatHashMapTest, Update) {
    FlatHashMap map(16);
    map.insert(100, 5);
    map.insert(100, 9); // update
    EXPECT_EQ(map.find(100), 9U);
    EXPECT_EQ(map.size(), 1U);
}

TEST(FlatHashMapTest, CollisionHandling) {
    // Insert many keys to force collisions
    FlatHashMap map(64);
    for (uint64_t i = 1; i <= 40; ++i) {
        EXPECT_TRUE(map.insert(i, static_cast<uint32_t>(i * 10)));
    }
    for (uint64_t i = 1; i <= 40; ++i) {
        EXPECT_EQ(map.find(i), static_cast<uint32_t>(i * 10));
    }
}

TEST(FlatHashMapTest, EraseAndReLookup) {
    // Ensure backward-shift deletion doesn't break subsequent lookups
    FlatHashMap map(16);
    // Insert keys that likely collide
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    map.erase(1);
    EXPECT_EQ(map.find(2), 20U);
    EXPECT_EQ(map.find(3), 30U);
}

// =====================================================================
// BookSide Tests
// =====================================================================
TEST(BookSideTest, InitialState) {
    BookSide<32> bid_side(0);
    EXPECT_EQ(bid_side.avl_root(), kInvalidPoolIndex);
    EXPECT_FALSE(bid_side.has_bbo());
    EXPECT_EQ(bid_side.level_count(), 0U);
    EXPECT_EQ(bid_side.side(), 0U);
}

TEST(BookSideTest, BestBidEmptyReturnsInvalid) {
    PriceLevelPool<32> pool;
    BookSide<32> bid_side(0);
    auto bb = bid_side.best_bid(pool);
    EXPECT_FALSE(bb.valid);
}
