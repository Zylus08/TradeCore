#include <benchmark/benchmark.h>
#include "tradecore/memory/object_pool.hpp"
#include <memory>
#include <vector>

using namespace tradecore::memory;

struct DummyOrder {
    uint64_t id;
    uint32_t price;
    uint32_t qty;
    uint8_t side;
    uint8_t padding[47]; // Pad to 64 bytes
};

static void BM_SystemMalloc(benchmark::State& state) {
    for (auto _ : state) {
        auto* order = new DummyOrder{};
        benchmark::DoNotOptimize(order);
        delete order;
    }
}
BENCHMARK(BM_SystemMalloc);

static void BM_ObjectPool(benchmark::State& state) {
    ObjectPool<DummyOrder, 10000> pool;
    for (auto _ : state) {
        auto* order = pool.allocate();
        benchmark::DoNotOptimize(order);
        pool.deallocate(order);
    }
}
BENCHMARK(BM_ObjectPool);

BENCHMARK_MAIN();
