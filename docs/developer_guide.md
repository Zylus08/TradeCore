# TradeCore Developer Guide

Welcome to TradeCore! This guide will get you from `git clone` to actively profiling engine metrics in under 5 minutes.

## 1. Quick Start (Build & Test)

TradeCore uses CMake and requires a C++20 compliant compiler (GCC 11+ or Clang 14+ recommended).

```bash
# Clone the repository
git clone https://github.com/TradeCore/TradeCore.git
cd TradeCore

# Configure and Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j $(nproc)

# Run the complete test suite
ctest --output-on-failure
```

## 2. Running Benchmarks & Performance Contracts

TradeCore considers performance to be an enforceable contract. Running benchmarks locally allows you to verify that your code changes haven't introduced regressions.

```bash
# From the build directory
./benchmarks/tradecore_benchmarks
```
*Note: Our CI pipeline will automatically run these benchmarks against static thresholds (P99 bounds) and fail your PR if the contract is breached.*

## 3. Replay Engine

The Replay Engine is the single source of truth for deterministic execution and telemetry extraction. 
To run a PCAP or simulated ITCH feed through the core:

*(Example usage snippet, full CLI interface pending Phase 6)*
```cpp
#include "tradecore/replay/replay_engine.hpp"

// Initialize memory-backed or PCAP DataSource
MemoryDataSource source(historical_payloads);

// Spin up Matcher & Telemetry
MatchingEngine<10000, 1000> matcher;
MetricsRegistry telemetry;

// Instantiate Replay at Maximum Throughput
ReplayEngine engine(source, matcher, &telemetry, PlaybackMode::MaxThroughput);
engine.run();

// Dump Performance
telemetry.end_to_end_latency.percentile(99.0);
```

## 4. Extending the Engine

### Adding a new Risk Rule
Because `RiskPipeline` uses variadic templates, you simply define a functor class and inject it into the pipeline instantiation. No inheritance is required.

```cpp
class MyNewRiskRule {
public:
    [[nodiscard]] RiskResult evaluate(const OrderAction& order) const noexcept {
        if (/* violation */) return {false, "Violation reason"};
        return {true, ""};
    }
};

// Injection:
RiskPipeline<MaxOrderSizeCheck, MyNewRiskRule> pipeline(...);
```

### Submitting a Pull Request
Please ensure your PR passes the CI checklist automatically triggered on commit:
1. `ctest` passes 100%
2. `tradecore_benchmarks` falls strictly within the Performance Contract.
3. No warnings from `clang-tidy`.
4. Sanitizers (ASan/UBSan) report 0 findings.
