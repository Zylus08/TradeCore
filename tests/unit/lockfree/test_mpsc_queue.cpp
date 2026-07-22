#include <gtest/gtest.h>
#include "tradecore/lockfree/mpsc_queue.hpp"
#include <thread>
#include <vector>

using namespace tradecore::lockfree;

TEST(MPSCQueueTest, BasicPushPop) {
    MPSCQueue<int, 16, 2> queue; // 2 producers

    EXPECT_TRUE(queue.push(0, 100)); // Producer 0
    EXPECT_TRUE(queue.push(1, 200)); // Producer 1

    int val;
    // Consumer polls round robin, should see both
    EXPECT_TRUE(queue.pop(val));
    EXPECT_TRUE(queue.pop(val));
    EXPECT_FALSE(queue.pop(val));
}

TEST(MPSCQueueTest, Threaded) {
    MPSCQueue<int, 1024, 4> queue;
    constexpr int kNumItemsPerProducer = 50000;
    
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < kNumItemsPerProducer; ++i) {
                while (!queue.push(p, p * 100000 + i)) {
                    // spin
                }
            }
        });
    }

    int received_count = 0;
    std::thread consumer([&]() {
        while (received_count < 4 * kNumItemsPerProducer) {
            int val;
            if (queue.pop(val)) {
                received_count++;
            }
        }
    });

    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    EXPECT_EQ(received_count, 4 * kNumItemsPerProducer);
}
