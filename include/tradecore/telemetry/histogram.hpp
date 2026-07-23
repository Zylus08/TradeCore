#pragma once

#include <cstdint>
#include <array>
#include <algorithm>
#include <numeric>

namespace tradecore::telemetry {

// A high-performance, wait-free latency histogram optimized for microsecond bounds.
// Uses exact 1ns resolution for the first MaxValueNs.
template <uint32_t MaxValueNs = 10000>
class Histogram {
public:
    Histogram() noexcept {
        reset();
    }

    void record(uint64_t value_ns) noexcept {
        if (value_ns < MaxValueNs) {
            buckets_[value_ns]++;
        } else {
            overflow_count_++;
        }
        
        count_++;
        sum_ns_ += value_ns;
        
        if (value_ns > max_ns_) { max_ns_ = value_ns; }
        if (value_ns < min_ns_) { min_ns_ = value_ns; }
    }

    void reset() noexcept {
        buckets_.fill(0);
        overflow_count_ = 0;
        count_ = 0;
        sum_ns_ = 0;
        max_ns_ = 0;
        min_ns_ = static_cast<uint64_t>(-1);
    }

    [[nodiscard]] uint64_t count() const noexcept { return count_; }
    [[nodiscard]] uint64_t sum() const noexcept { return sum_ns_; }
    [[nodiscard]] uint64_t max() const noexcept { return count_ == 0 ? 0 : max_ns_; }
    [[nodiscard]] uint64_t min() const noexcept { return count_ == 0 ? 0 : min_ns_; }
    [[nodiscard]] uint64_t overflow_count() const noexcept { return overflow_count_; }

    [[nodiscard]] uint64_t mean() const noexcept {
        return count_ == 0 ? 0 : sum_ns_ / count_;
    }

    // Calculates percentile. percentile should be [0.0, 100.0]
    [[nodiscard]] uint64_t percentile(double p) const noexcept {
        if (count_ == 0) return 0;
        if (p <= 0.0) return min();
        if (p >= 100.0) return max();

        uint64_t target_count = static_cast<uint64_t>((p / 100.0) * static_cast<double>(count_));
        if (target_count == 0) target_count = 1;

        uint64_t cumulative = 0;
        for (std::size_t i = 0; i < MaxValueNs; ++i) {
            cumulative += buckets_[i];
            if (cumulative >= target_count) {
                return i;
            }
        }

        // If it falls in the overflow bucket, we just return the tracked max
        return max();
    }

private:
    std::array<uint64_t, MaxValueNs> buckets_;
    uint64_t overflow_count_;
    uint64_t count_;
    uint64_t sum_ns_;
    uint64_t max_ns_;
    uint64_t min_ns_;
};

} // namespace tradecore::telemetry
