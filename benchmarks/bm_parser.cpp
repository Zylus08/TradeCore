#include <benchmark/benchmark.h>
#include "tradecore/parser/itch_parser.hpp"
#include "tradecore/parser/parser_context.hpp"
#include "tradecore/memory/arena_allocator.hpp"
#include <vector>
#include <cstring>

using namespace tradecore::parser;
using namespace tradecore::memory;

static void BM_ITCHParser_AddOrder(benchmark::State& state) {
    alignas(64) std::byte arena_buf[1024 * 1024];
    ArenaAllocator arena(sizeof(arena_buf), arena_buf);
    TimestampSource time_src;
    SymbolTable sym_tab;
    Statistics stats;

    ParserContext ctx{arena, time_src, sym_tab, stats};
    ITCHParser parser;

    itch::AddOrderMsg msg{};
    msg.type = 'A';
    msg.stock_locate = 0;
    msg.tracking_number = 0;
    msg.timestamp = tradecore::host_to_net(123456789ULL);
    msg.order_reference_number = tradecore::host_to_net(999ULL);
    msg.buy_sell_indicator = 'B';
    msg.shares = tradecore::host_to_net(100U);
    std::memcpy(msg.stock, "AAPL    ", 8);
    msg.price = tradecore::host_to_net(1500000U);

    std::vector<std::byte> buffer(sizeof(itch::AddOrderMsg));
    std::memcpy(buffer.data(), &msg, sizeof(itch::AddOrderMsg));
    std::span<const std::byte> payload(buffer);

    for (auto _ : state) {
        auto result = parser.parse(ctx, payload);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ITCHParser_AddOrder);

BENCHMARK_MAIN();
