#include <gtest/gtest.h>
#include "tradecore/orderbook/order_book_verifier.hpp"

using namespace tradecore::orderbook;

// Helpers to build minimal level/order arrays for testing
static Order make_order(uint64_t id, uint64_t ts, uint32_t price, uint32_t qty,
                        uint8_t side, uint32_t next = kInvalidPoolIndex)
{
    Order o{};
    o.order_id        = id;
    o.timestamp_ns    = ts;
    o.price           = price;
    o.qty             = qty;
    o.remaining_qty   = qty;
    o.side            = side;
    o.state           = OrderState::Active;
    o.next_in_level   = next;
    o.prev_in_level   = kInvalidPoolIndex;
    return o;
}

static PriceLevel make_level(uint32_t price, uint32_t qty, uint32_t count,
                              uint32_t head, uint8_t side,
                              int8_t balance = 0)
{
    PriceLevel l{};
    l.price          = price;
    l.total_qty      = qty;
    l.order_count    = count;
    l.head_order_idx = head;
    l.tail_order_idx = head;
    l.left_child     = kInvalidPoolIndex;
    l.right_child    = kInvalidPoolIndex;
    l.parent         = kInvalidPoolIndex;
    l.balance        = balance;
    l.side           = side;
    return l;
}

// --- Happy Path ---
TEST(OrderBookVerifierTest, ValidBook) {
    // Two bids (descending), one ask. No crossing. Correct qtys. Correct FIFO.
    std::vector<PriceLevel> bids = {
        make_level(200, 100, 1, 0, 0),
        make_level(100, 50,  1, 1, 0),
    };
    std::vector<PriceLevel> asks = {
        make_level(300, 200, 1, 2, 1),
    };
    std::vector<Order> pool = {
        make_order(1001, 1000, 200, 100, 0),
        make_order(1002, 1001, 100, 50,  0),
        make_order(1003, 1002, 300, 200, 1),
    };
    std::unordered_map<uint64_t, uint32_t> map = {{1001,0},{1002,1},{1003,2}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex, 0, 0);
    EXPECT_TRUE(res.ok) << res.first_failure;
}

// --- Crossed Book ---
TEST(OrderBookVerifierTest, CrossedBook) {
    std::vector<PriceLevel> bids = { make_level(500, 100, 1, 0, 0) };
    std::vector<PriceLevel> asks = { make_level(400, 100, 1, 1, 1) };
    std::vector<Order> pool = {
        make_order(1, 100, 500, 100, 0),
        make_order(2, 101, 400, 100, 1),
    };
    std::unordered_map<uint64_t, uint32_t> map = {{1,0},{2,1}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex, 0, 0);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.first_failure, "Crossed book: best_bid.price >= best_ask.price");
}

// --- Level Quantity Mismatch ---
TEST(OrderBookVerifierTest, LevelQtyMismatch) {
    std::vector<PriceLevel> bids = { make_level(100, 999, 1, 0, 0) }; // wrong qty
    std::vector<PriceLevel> asks;
    std::vector<Order> pool = { make_order(1, 100, 100, 50, 0) };
    std::unordered_map<uint64_t, uint32_t> map = {{1,0}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex,
                                         kInvalidPoolIndex, kInvalidPoolIndex);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.first_failure, "Level total_qty != sum(order.remaining_qty)");
}

// --- FIFO Violation ---
TEST(OrderBookVerifierTest, FIFOViolation) {
    // ts goes backwards: order 0 has ts=2000, order 1 (its next) has ts=1000
    std::vector<PriceLevel> bids = { make_level(100, 150, 2, 0, 0) };
    std::vector<PriceLevel> asks;

    std::vector<Order> pool(2);
    pool[0] = make_order(1, 2000, 100, 100, 0, 1); // ts=2000, points to 1
    pool[1] = make_order(2, 1000, 100, 50,  0);    // ts=1000 (older = FIFO violated)
    pool[0].next_in_level = 1;

    std::unordered_map<uint64_t, uint32_t> map = {{1,0},{2,1}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex,
                                         kInvalidPoolIndex, kInvalidPoolIndex);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.first_failure,
              "FIFO violated: timestamp not monotonic within price level");
}

// --- Duplicate Order ID ---
TEST(OrderBookVerifierTest, DuplicateOrderId) {
    std::vector<PriceLevel> bids = { make_level(100, 100, 1, 0, 0) };
    std::vector<PriceLevel> asks = { make_level(200, 100, 1, 1, 1) };

    std::vector<Order> pool = {
        make_order(42, 100, 100, 100, 0), // order_id 42
        make_order(42, 101, 200, 100, 1), // duplicate order_id 42
    };
    std::unordered_map<uint64_t, uint32_t> map = {{42,0}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex,
                                         kInvalidPoolIndex, kInvalidPoolIndex);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.first_failure, "Duplicate order_id detected");
}

// --- Orphaned Order: in map, in no level ---
// Simulates a bug where cancel removes an order from the level's linked list
// but forgets to erase it from the hash map.
TEST(OrderBookVerifierTest, OrphanedOrder) {
    std::vector<PriceLevel> bids = { make_level(100, 100, 1, 0, 0) };
    std::vector<PriceLevel> asks;

    std::vector<Order> pool = {
        make_order(1, 100, 100, 100, 0), // pool[0]: in bid level, OK
        make_order(2, 101, 100, 50,  0), // pool[1]: NOT in any level
    };
    // Map incorrectly still references pool[1]
    std::unordered_map<uint64_t, uint32_t> map = {{1, 0}, {2, 1}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex,
                                         kInvalidPoolIndex, kInvalidPoolIndex);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.first_failure, "Order in hash map belongs to no price level (orphaned)");
}

// --- Ghost Order: reachable from a level, absent from map ---
// Simulates a bug where add_order inserts into a level's linked list
// but forgets to register the order in the hash map.
TEST(OrderBookVerifierTest, GhostOrder) {
    std::vector<Order> pool(2);
    pool[0] = make_order(1, 100, 100, 100, 0, 1); // head, points to pool[1]
    pool[1] = make_order(2, 101, 100, 50,  0);    // tail, ghost: not in map

    std::vector<PriceLevel> bids = { make_level(100, 150, 2, 0, 0) };
    bids[0].tail_order_idx = 1;
    std::vector<PriceLevel> asks;

    // Map only knows about order 1, not order 2
    std::unordered_map<uint64_t, uint32_t> map = {{1, 0}};

    auto res = OrderBookVerifier::verify(bids, asks, pool, map,
                                         kInvalidPoolIndex, kInvalidPoolIndex,
                                         kInvalidPoolIndex, kInvalidPoolIndex);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.first_failure,
              "Order reachable from price level is absent from hash map (ghost)");
}

