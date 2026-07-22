#include <gtest/gtest.h>
#include "tradecore/memory/object_pool.hpp"

using namespace tradecore::memory;

struct Dummy {
    int id{0};
    double value{0.0};
    
    Dummy() = default;
    Dummy(int i, double v) : id(i), value(v) {}
};

TEST(ObjectPoolTest, BasicAllocation) {
    ObjectPool<Dummy, 10> pool;
    
    EXPECT_EQ(pool.active_count(), 0);
    EXPECT_EQ(pool.capacity(), 10);
    
    auto* d1 = pool.allocate();
    ASSERT_NE(d1, nullptr);
    d1->id = 1;
    
    EXPECT_EQ(pool.active_count(), 1);
    
    auto* d2 = pool.allocate(2, 3.14);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->id, 2);
    EXPECT_EQ(d2->value, 3.14);
    
    EXPECT_EQ(pool.active_count(), 2);
}

TEST(ObjectPoolTest, Deallocation) {
    ObjectPool<Dummy, 10> pool;
    
    auto* d1 = pool.allocate();
    auto* d2 = pool.allocate();
    
    EXPECT_EQ(pool.active_count(), 2);
    
    pool.deallocate(d1);
    EXPECT_EQ(pool.active_count(), 1);
    
    // Allocate again, should reuse the slot
    auto* d3 = pool.allocate();
    EXPECT_EQ(d1, d3); // Pointer should be identical
    EXPECT_EQ(pool.active_count(), 2);
}

TEST(ObjectPoolTest, Exhaustion) {
    ObjectPool<Dummy, 2> pool;
    
    auto* d1 = pool.allocate();
    auto* d2 = pool.allocate();
    
    ASSERT_NE(d1, nullptr);
    ASSERT_NE(d2, nullptr);
    
    // Pool is full
    auto* d3 = pool.allocate();
    EXPECT_EQ(d3, nullptr);
}

TEST(ObjectPoolTest, Reset) {
    ObjectPool<Dummy, 5> pool;
    
    for (int i = 0; i < 5; ++i) {
        pool.allocate();
    }
    
    EXPECT_EQ(pool.active_count(), 5);
    EXPECT_EQ(pool.allocate(), nullptr);
    
    pool.reset();
    EXPECT_EQ(pool.active_count(), 0);
    
    auto* d1 = pool.allocate();
    ASSERT_NE(d1, nullptr); // Should succeed after reset
}
