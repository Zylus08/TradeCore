#include <benchmark/benchmark.h>
#include "tradecore/matching/matching_engine.hpp"

using namespace tradecore::matching;

static constexpr std::size_t kMaxOrders = 10000;
static constexpr std::size_t kMaxLevels = 1000;

static void BM_Match_SingleLevel(benchmark::State& state) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    // Add resting ask
    OrderAction ask{};
    ask.order_id = 1;
    ask.price = 1000;
    ask.qty = 100;
    ask.side = 1;
    ask.type = OrderType::Limit;
    
    // Incoming bid
    OrderAction bid{};
    bid.price = 1000;
    bid.qty = 50;
    bid.side = 0;
    bid.type = OrderType::Limit;

    uint64_t next_id = 2;

    for (auto _ : state) {
        state.PauseTiming();
        ask.order_id = next_id++;
        ask.qty = 100;
        engine.match(ask); // replenish ask
        
        bid.order_id = next_id++;
        state.ResumeTiming();
        
        auto result = engine.match(bid); // Matches exactly one order
        benchmark::DoNotOptimize(result);
        
        state.PauseTiming();
        if (next_id > kMaxOrders - 100) {
            engine = MatchingEngine<kMaxOrders, kMaxLevels>{};
            next_id = 1;
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_Match_SingleLevel);

static void BM_Match_MultiLevelSweep(benchmark::State& state) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    
    OrderAction ask{};
    ask.side = 1;
    ask.type = OrderType::Limit;
    
    OrderAction bid{};
    bid.side = 0;
    bid.type = OrderType::Limit;
    
    uint64_t next_id = 1;
    
    for (auto _ : state) {
        state.PauseTiming();
        // Setup 10 deep levels of asks, each qty=10
        for (uint32_t i = 0; i < 10; ++i) {
            ask.order_id = next_id++;
            ask.price = 1000 + i;
            ask.qty = 10;
            engine.match(ask);
        }
        
        bid.order_id = next_id++;
        bid.price = 1010; // Sweep through all 10 levels
        bid.qty = 100; 
        state.ResumeTiming();
        
        auto result = engine.match(bid); // Matches 10 orders across 10 levels
        benchmark::DoNotOptimize(result);
        
        state.PauseTiming();
        if (next_id > kMaxOrders - 100) {
            engine = MatchingEngine<kMaxOrders, kMaxLevels>{};
            next_id = 1;
        }
        state.ResumeTiming();
    }
}
BENCHMARK(BM_Match_MultiLevelSweep);

static void BM_RiskPipeline(benchmark::State& state) {
    MatchingEngine<kMaxOrders, kMaxLevels> engine;
    engine.add_risk_check(std::make_unique<MaxOrderSizeCheck>(1000));
    
    OrderAction bid{};
    bid.order_id = 1;
    bid.price = 1000;
    bid.qty = 50;
    bid.side = 0;
    bid.type = OrderType::Limit;

    for (auto _ : state) {
        auto result = engine.match(bid); // Hits risk check and inserts into book
        benchmark::DoNotOptimize(result);
        
        state.PauseTiming();
        engine = MatchingEngine<kMaxOrders, kMaxLevels>{};
        engine.add_risk_check(std::make_unique<MaxOrderSizeCheck>(1000));
        state.ResumeTiming();
    }
}
BENCHMARK(BM_RiskPipeline);

BENCHMARK_MAIN();
