#include <gtest/gtest.h>
#include "tradecore/orderbook/book_snapshot.hpp"

using namespace tradecore::orderbook;

TEST(BookSnapshotTest, EmptySnapshot) {
    BookSnapshot snap{};
    EXPECT_FALSE(snap.has_bbo());
    EXPECT_EQ(snap.spread(), 0U);
    EXPECT_EQ(snap.mid_price(), 0U);
    EXPECT_EQ(snap.bid_depth(), 0U);
    EXPECT_EQ(snap.ask_depth(), 0U);
}

TEST(BookSnapshotTest, WithBBO) {
    BookSnapshot snap{};
    snap.best_bid = BestBid{.price = 100, .qty = 50, .order_count = 2, .valid = true};
    snap.best_ask = BestAsk{.price = 102, .qty = 30, .order_count = 1, .valid = true};

    EXPECT_TRUE(snap.has_bbo());
    EXPECT_EQ(snap.spread(), 2U);
    EXPECT_EQ(snap.mid_price(), 101U);
}

TEST(BookSnapshotTest, DepthView) {
    PriceLevel bid_levels[2]{};
    bid_levels[0].price = 100;
    bid_levels[1].price = 99;

    PriceLevel ask_levels[1]{};
    ask_levels[0].price = 102;

    BookSnapshot snap{};
    snap.bids = std::span<const PriceLevel>(bid_levels, 2);
    snap.asks = std::span<const PriceLevel>(ask_levels, 1);

    EXPECT_EQ(snap.bid_depth(), 2U);
    EXPECT_EQ(snap.ask_depth(), 1U);
    EXPECT_EQ(snap.bids[0].price, 100U);
    EXPECT_EQ(snap.asks[0].price, 102U);
}

// Verify BookObserver concept compiles correctly
struct MockObserver {
    int call_count{0};
    void on_book_update(const BookSnapshot&) { call_count++; }
};
static_assert(BookObserver<MockObserver>,
              "MockObserver must satisfy BookObserver concept");

TEST(BookSnapshotTest, ObserverCallable) {
    BookSnapshot snap{};
    MockObserver obs;
    obs.on_book_update(snap);
    EXPECT_EQ(obs.call_count, 1);
}
