#pragma once

#include "tradecore/telemetry/histogram.hpp"
#include <chrono>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

namespace tradecore::telemetry {

// Platform-specific high-resolution hardware timestamp
inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    return __rdtsc();
#else
    // Fallback if not x86
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

// Convert cycles to nanoseconds (requires calibration in a real system)
// For simplicity in this demo, we fall back to steady_clock if we want accurate ns.
inline uint64_t get_time_ns() noexcept {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

// RAII timer that records elapsed time into a provided histogram
template <uint32_t MaxValueNs = 10000>
class LatencyTracker {
public:
    explicit LatencyTracker(Histogram<MaxValueNs>& hist) noexcept 
        : hist_(hist), start_ns_(get_time_ns()) {}

    ~LatencyTracker() {
        uint64_t end_ns = get_time_ns();
        hist_.record(end_ns - start_ns_);
    }

    LatencyTracker(const LatencyTracker&) = delete;
    LatencyTracker& operator=(const LatencyTracker&) = delete;

private:
    Histogram<MaxValueNs>& hist_;
    uint64_t start_ns_;
};

} // namespace tradecore::telemetry
