#include <benchmark/benchmark.h>
#include "tradecore/orderbook/order_book.hpp"
#include "tradecore/parser/book_update.hpp"

using namespace tradecore::orderbook;
using namespace tradecore::parser;

// Define a reasonable max capacity for the benchmarks
static constexpr std::size_t kMaxOrders = 100000;
static constexpr std::size_t kMaxLevels = 10000;

static void BM_AddOrder_ExistingLevel(benchmark::State& state) {
    OrderBook<kMaxOrders, kMaxLevels> book;

    // Create a base level at price 1000
    BookUpdate initial_add{};
    initial_add.type = UpdateType::AddOrder;
    initial_add.side = 0; // Bid
    initial_add.price = 1000;
    initial_add.qty = 100;
    initial_add.order_id = 1;
    initial_add.timestamp_ns = 1000;
    
    book.dispatch(initial_add);

    uint64_t next_order_id = 2;

    for (auto _ : state) {
        BookUpdate add{};
        add.type = UpdateType::AddOrder;
        add.side = 0;
        add.price = 1000; // Same price -> existing level
        add.qty = 50;
        add.order_id = next_order_id++;
        add.timestamp_ns = 2000;

        book.dispatch(add);
    }
}
BENCHMARK(BM_AddOrder_ExistingLevel);

static void BM_AddOrder_NewLevel(benchmark::State& state) {
    OrderBook<kMaxOrders, kMaxLevels> book;

    uint64_t next_order_id = 1;
    uint32_t next_price = 1; // Different price every time

    for (auto _ : state) {
        BookUpdate add{};
        add.type = UpdateType::AddOrder;
        add.side = 0;
        add.price = next_price++;
        add.qty = 50;
        add.order_id = next_order_id++;
        add.timestamp_ns = 1000;

        book.dispatch(add);

        // Periodically clear to prevent pool exhaustion during long benchmark runs
        if (next_order_id > kMaxLevels - 10) {
            state.PauseTiming();
            book = OrderBook<kMaxOrders, kMaxLevels>{}; // reset
            next_order_id = 1;
            next_price = 1;
            state.ResumeTiming();
        }
    }
}
BENCHMARK(BM_AddOrder_NewLevel);

static void BM_CancelOrder(benchmark::State& state) {
    OrderBook<kMaxOrders, kMaxLevels> book;
    BookUpdate add{};
    add.type = UpdateType::AddOrder;
    add.side = 0;
    add.price = 1000;
    add.order_id = 1;

    for (auto _ : state) {
        state.PauseTiming();
        add.qty = 100;
        add.timestamp_ns = 1000;
        book.dispatch(add);
        
        BookUpdate cancel{};
        cancel.type = UpdateType::CancelOrder;
        cancel.order_id = 1;
        cancel.qty = 50; // Partial cancel
        state.ResumeTiming();
        
        book.dispatch(cancel);
        
        state.PauseTiming();
        BookUpdate del{};
        del.type = UpdateType::DeleteOrder;
        del.order_id = 1;
        book.dispatch(del);
        state.ResumeTiming();
    }
}
BENCHMARK(BM_CancelOrder);

static void BM_DeleteOrder(benchmark::State& state) {
    OrderBook<kMaxOrders, kMaxLevels> book;
    BookUpdate add{};
    add.type = UpdateType::AddOrder;
    add.side = 0;
    add.price = 1000;
    add.order_id = 1;

    for (auto _ : state) {
        state.PauseTiming();
        add.qty = 100;
        book.dispatch(add);
        
        BookUpdate del{};
        del.type = UpdateType::DeleteOrder;
        del.order_id = 1;
        state.ResumeTiming();
        
        book.dispatch(del);
    }
}
BENCHMARK(BM_DeleteOrder);

static void BM_ReplaceOrder(benchmark::State& state) {
    OrderBook<kMaxOrders, kMaxLevels> book;
    BookUpdate add{};
    add.type = UpdateType::AddOrder;
    add.side = 0;
    add.price = 1000;
    add.order_id = 1;
    add.qty = 100;

    book.dispatch(add);

    uint64_t current_id = 1;
    uint64_t next_id = 2;

    for (auto _ : state) {
        BookUpdate replace{};
        replace.type = UpdateType::ReplaceOrder;
        replace.order_id = current_id;
        replace.match_id = next_id;
        replace.price = 1005; // Change price
        replace.qty = 50;
        replace.side = 0;
        
        book.dispatch(replace);
        
        current_id = next_id;
        next_id++;
        
        if (next_id > kMaxLevels - 10) {
            state.PauseTiming();
            book = OrderBook<kMaxOrders, kMaxLevels>{}; // reset
            current_id = 1;
            next_id = 2;
            add.order_id = 1;
            book.dispatch(add);
            state.ResumeTiming();
        }
    }
}
BENCHMARK(BM_ReplaceOrder);

BENCHMARK_MAIN();
