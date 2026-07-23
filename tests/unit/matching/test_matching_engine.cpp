#include <gtest/gtest.h>
#include "tradecore/matching/matching_engine.hpp"

using namespace tradecore::matching;

static constexpr std::size_t kMaxOrders = 10000;
static constexpr std::size_t kMaxLevels = 1000;

TEST(MatchingEngineTest, LimitOrderAddWithoutMatch) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction action{};
    action.order_id = 1;
    action.client_id = 100;
    action.price = 1000;
    action.qty = 50;
    action.side = 0; // Buy
    action.type = OrderType::Limit;

    auto result = engine.match(action);
    EXPECT_EQ(result.status, ExecutionStatus::Accepted);
    EXPECT_TRUE(result.trades.empty());

    // Verify book state
    const auto& book = engine.book();
    EXPECT_TRUE(book.bids().has_bbo());
    EXPECT_EQ(book.bids().bbo_level(book.level_pool())->price, 1000);
    EXPECT_EQ(book.bids().bbo_level(book.level_pool())->total_qty, 50);
}

TEST(MatchingEngineTest, LimitOrderFullFill) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask{};
    ask.order_id = 1;
    ask.price = 1000;
    ask.qty = 50;
    ask.side = 1; // Sell
    ask.type = OrderType::Limit;
    engine.match(ask);

    OrderAction bid{};
    bid.order_id = 2;
    bid.price = 1000;
    bid.qty = 50;
    bid.side = 0; // Buy
    bid.type = OrderType::Limit;
    
    auto result = engine.match(bid);
    EXPECT_EQ(result.status, ExecutionStatus::Filled);
    ASSERT_EQ(result.trades.size(), 1);
    
    EXPECT_EQ(result.trades[0].aggressor_order_id, 2);
    EXPECT_EQ(result.trades[0].resting_order_id, 1);
    EXPECT_EQ(result.trades[0].price, 1000);
    EXPECT_EQ(result.trades[0].quantity, 50);

    const auto& book = engine.book();
    EXPECT_FALSE(book.asks().has_bbo());
    EXPECT_FALSE(book.bids().has_bbo());
}

TEST(MatchingEngineTest, LimitOrderPartialFillLeavesResidual) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask{};
    ask.order_id = 1;
    ask.price = 1000;
    ask.qty = 50;
    ask.side = 1;
    ask.type = OrderType::Limit;
    engine.match(ask);

    OrderAction bid{};
    bid.order_id = 2;
    bid.price = 1000;
    bid.qty = 100;
    bid.side = 0;
    bid.type = OrderType::Limit;
    
    auto result = engine.match(bid);
    EXPECT_EQ(result.status, ExecutionStatus::PartiallyFilled);
    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 50);

    const auto& book = engine.book();
    EXPECT_FALSE(book.asks().has_bbo());
    EXPECT_TRUE(book.bids().has_bbo());
    EXPECT_EQ(book.bids().bbo_level(book.level_pool())->price, 1000);
    EXPECT_EQ(book.bids().bbo_level(book.level_pool())->total_qty, 50);
}

TEST(MatchingEngineTest, MarketOrderAggressiveSweep) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask1{};
    ask1.order_id = 1;
    ask1.price = 1000;
    ask1.qty = 50;
    ask1.side = 1;
    ask1.type = OrderType::Limit;
    engine.match(ask1);

    OrderAction ask2{};
    ask2.order_id = 2;
    ask2.price = 1010;
    ask2.qty = 50;
    ask2.side = 1;
    ask2.type = OrderType::Limit;
    engine.match(ask2);

    // Market order should sweep through both levels, ignoring price constraint
    OrderAction mkt{};
    mkt.order_id = 3;
    mkt.qty = 100;
    mkt.side = 0; // Buy
    mkt.type = OrderType::Market;

    auto result = engine.match(mkt);
    EXPECT_EQ(result.status, ExecutionStatus::Filled);
    ASSERT_EQ(result.trades.size(), 2);
    
    EXPECT_EQ(result.trades[0].price, 1000);
    EXPECT_EQ(result.trades[0].quantity, 50);
    EXPECT_EQ(result.trades[1].price, 1010);
    EXPECT_EQ(result.trades[1].quantity, 50);

    const auto& book = engine.book();
    EXPECT_FALSE(book.asks().has_bbo()); // Book should be empty
}

TEST(MatchingEngineTest, IOCOrderDiscardsResidual) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask{};
    ask.order_id = 1;
    ask.price = 1000;
    ask.qty = 50;
    ask.side = 1;
    ask.type = OrderType::Limit;
    engine.match(ask);

    OrderAction ioc{};
    ioc.order_id = 2;
    ioc.price = 1000;
    ioc.qty = 100;
    ioc.side = 0; // Buy
    ioc.type = OrderType::IOC;

    auto result = engine.match(ioc);
    EXPECT_EQ(result.status, ExecutionStatus::PartiallyFilled);
    ASSERT_EQ(result.trades.size(), 1);
    
    const auto& book = engine.book();
    EXPECT_FALSE(book.asks().has_bbo()); 
    // The remaining 50 qty from IOC should NOT be on the book
    EXPECT_FALSE(book.bids().has_bbo()); 
}

TEST(MatchingEngineTest, FOKOrderPreScanSucceeds) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask{};
    ask.order_id = 1;
    ask.price = 1000;
    ask.qty = 100;
    ask.side = 1;
    ask.type = OrderType::Limit;
    engine.match(ask);

    OrderAction fok{};
    fok.order_id = 2;
    fok.price = 1000;
    fok.qty = 100; // Exact match
    fok.side = 0; 
    fok.type = OrderType::FOK;

    auto result = engine.match(fok);
    EXPECT_EQ(result.status, ExecutionStatus::Filled);
    ASSERT_EQ(result.trades.size(), 1);
}

TEST(MatchingEngineTest, FOKOrderPreScanFails) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask{};
    ask.order_id = 1;
    ask.price = 1000;
    ask.qty = 50;
    ask.side = 1;
    ask.type = OrderType::Limit;
    engine.match(ask);

    OrderAction fok{};
    fok.order_id = 2;
    fok.price = 1000;
    fok.qty = 100; // Not enough liquidity
    fok.side = 0; 
    fok.type = OrderType::FOK;

    auto result = engine.match(fok);
    // Should reject and do nothing
    EXPECT_EQ(result.status, ExecutionStatus::RejectedInvalid);
    EXPECT_TRUE(result.trades.empty());

    // Resting order should remain untouched
    const auto& book = engine.book();
    EXPECT_TRUE(book.asks().has_bbo());
    EXPECT_EQ(book.asks().bbo_level(book.level_pool())->total_qty, 50);
}

TEST(MatchingEngineTest, RiskPipelineCompileTime) {
    // Construct pipeline at compile time
    RiskPipeline<MaxOrderSizeCheck, FatFingerPriceCheck> pipeline(
        MaxOrderSizeCheck(100),
        FatFingerPriceCheck(100, 2000)
    );

    MatchingEngine<kMaxOrders, kMaxLevels, decltype(pipeline)> engine(pipeline);
    
    // Order within limits
    OrderAction action_ok{};
    action_ok.order_id = 1;
    action_ok.price = 1000;
    action_ok.qty = 100;
    action_ok.side = 0;
    action_ok.type = OrderType::Limit;
    auto res_ok = engine.match(action_ok);
    EXPECT_EQ(res_ok.status, ExecutionStatus::Accepted);

    // Fat finger fail
    OrderAction action_fat{};
    action_fat.order_id = 2;
    action_fat.price = 2001;
    action_fat.qty = 100;
    action_fat.side = 0;
    action_fat.type = OrderType::Limit;
    auto res_fat = engine.match(action_fat);
    EXPECT_EQ(res_fat.status, ExecutionStatus::RejectedRisk);
    
    // Size fail
    OrderAction action_size{};
    action_size.order_id = 3;
    action_size.price = 1000;
    action_size.qty = 101;
    action_size.side = 0;
    action_size.type = OrderType::Limit;
    auto res_size = engine.match(action_size);
    EXPECT_EQ(res_size.status, ExecutionStatus::RejectedRisk);
}
