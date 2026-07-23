#pragma once

#include "tradecore/telemetry/histogram.hpp"
#include <atomic>

namespace tradecore::telemetry {

// Centralized metrics container for the observability layer
class MetricsRegistry {
public:
    // Latency Histograms
    Histogram<10000> parser_latency;
    Histogram<10000> matcher_latency;
    Histogram<10000> risk_latency;
    Histogram<50000> replay_latency;
    Histogram<50000> end_to_end_latency;

    // Structural Counters
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> orders_processed{0};
    std::atomic<uint64_t> trades_generated{0};
    
    std::atomic<uint64_t> risk_rejects{0};
    std::atomic<uint64_t> ioc_cancels{0};
    std::atomic<uint64_t> fok_rejects{0};
    std::atomic<uint64_t> avl_rotations{0};
    
    // Call this periodically or on shutdown to print stats
    void reset() {
        parser_latency.reset();
        matcher_latency.reset();
        risk_latency.reset();
        replay_latency.reset();
        end_to_end_latency.reset();

        messages_processed = 0;
        orders_processed = 0;
        trades_generated = 0;
        risk_rejects = 0;
        ioc_cancels = 0;
        fok_rejects = 0;
        avl_rotations = 0;
    }
};

// Global or per-engine singleton. For zero-overhead we will inject this where needed.
} // namespace tradecore::telemetry
