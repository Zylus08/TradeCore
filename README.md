# TradeCore Matching Engine

TradeCore is an institutional-grade, zero-allocation Matching Engine designed for ultra-low latency execution and strictly deterministic market replay. Built in modern C++20, it is specifically architected to avoid the OS heap, virtual dispatch, and cache-line tearing on the hot execution path.

## 🚀 Key Features

* **Zero-Allocation Execution:** The primary matching loop (`ExecutionResult match(OrderAction)`) exclusively uses pre-allocated memory pools and inline `StaticVector` containers, ensuring 0 heap allocations and 0 GC jitter post-startup.
* **Deterministic Replay:** The custom `ReplayEngine` ensures identical $O(1)$ topological reconstruction of historical ITCH 5.0 PCAP data across multiple playback modes (MaxThroughput, RealTime, Accelerated, SingleStep).
* **Compile-Time Risk Pipeline:** Risk policy evaluation completely avoids virtual `vtable` lookups by unrolling verification checks at compile-time via C++17 variadic fold expressions.
* **Bounded Latency Telemetry:** The `tradecore::telemetry` layer uses lock-free, bounds-checked HDR-style histograms to provide true sub-nanosecond precision performance mapping (Parser, Matcher, Risk, E2E).

## 🧠 Architectural Overview

For deep dives into the engine's design, refer to our comprehensive institutional documentation suite:

* [Architecture & Pipeline Design](docs/architecture.md)
* [Memory Layout & Cache Optimization](docs/memory_layout.md)
* [Performance Guide & Tradeoffs](docs/performance_guide.md)
* [Developer Onboarding Guide](docs/developer_guide.md)

## ⚡ Performance Contracts

TradeCore enforces statistical latency bounds mathematically via CI. We do not just report benchmarks; we fail builds that break these thresholds.

| Component | Target Contract (P99) |
|---|---|
| ITCH Parsing | < 120 ns |
| Book Mutation | < 140 ns |
| Matching (Limit) | < 200 ns |
| Matching (Sweep) | < 250 ns |

## 🛡️ Validation Matrix

To guarantee institutional-grade safety, TradeCore implements a layered topological verification matrix.

| Verification Layer | Invocation | Scope | Pass Criteria |
|---|---|---|---|
| `TRADECORE_VERIFY_BOOK` | Debug / Tests | Asserts bidirectional AVL parent pointers, correct queue depths, and $O(1)$ BBO caches across the entire OrderBook topology. | Zero assertion failures |
| `TRADECORE_VERIFY_MATCH` | Per Execution Sweep | Validates size and price bijectivity during atomic limit sweeps. Guarantees price improvement belongs to the aggressor. | Zero assertion failures |
| `TRADECORE_VERIFY_EXECUTION` | Post Execution Pipeline | Macro-level check guaranteeing cumulative total executed volume plus residual volume exactly matches initial incoming volume. | Zero assertion failures |
| `BookDigest` | E2E Replay Validation | Deterministic FNV-1a state hash generated from an exact in-order tree traversal. | Successive runs yield identical digests |

## 🛠️ Quick Start

```bash
git clone https://github.com/Zylus08/TradeCore.git
cd TradeCore
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j $(nproc)
ctest --output-on-failure
```

For detailed usage, see the [Developer Guide](docs/developer_guide.md).

## 📄 License & Contributing

* Please see [CONTRIBUTING.md](CONTRIBUTING.md) for pull request guidelines.
* Licensed under the MIT License. See [LICENSE](LICENSE) for details.
