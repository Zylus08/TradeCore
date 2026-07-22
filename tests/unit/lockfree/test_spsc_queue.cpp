#include <gtest/gtest.h>
#include "tradecore/lockfree/spsc_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace tradecore::lockfree;

TEST(SPSCQueueTest, BasicPushPop) {
    SPSCQueue<int, 16> queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    EXPECT_TRUE(queue.push(42));
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);

    int val = 0;
    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(queue.empty());
}

TEST(SPSCQueueTest, FullAndEmpty) {
    SPSCQueue<int, 4> queue;

    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_TRUE(queue.push(4));

    // Queue is full
    EXPECT_FALSE(queue.push(5));
    EXPECT_EQ(queue.size(), 4);

    int val;
    EXPECT_TRUE(queue.pop(val)); EXPECT_EQ(val, 1);
    EXPECT_TRUE(queue.pop(val)); EXPECT_EQ(val, 2);
    EXPECT_TRUE(queue.pop(val)); EXPECT_EQ(val, 3);
    EXPECT_TRUE(queue.pop(val)); EXPECT_EQ(val, 4);

    // Queue is empty
    EXPECT_FALSE(queue.pop(val));
}

TEST(SPSCQueueTest, Threaded) {
    SPSCQueue<int, 1024> queue;
    constexpr int kNumItems = 100000;

    std::thread producer([&]() {
        for (int i = 0; i < kNumItems; ++i) {
            while (!queue.push(i)) {
                // spin
            }
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < kNumItems; ++i) {
            int val = -1;
            while (!queue.pop(val)) {
                // spin
            }
            EXPECT_EQ(val, i);
        }
    });

    producer.join();
    consumer.join();
}
