#pragma once

#include <cstdint>
#include <chrono>
#include <thread>
#include "tradecore/common/assert.hpp"

namespace tradecore::replay {

enum class PlaybackMode {
    RealTime,       // Delay to match embedded timestamp spacing
    Accelerated,    // Speed up time by Nx factor
    MaxThroughput,  // Zero delay between messages
    SingleStep      // Advance manually (driven externally)
};

class ReplayClock {
public:
    explicit ReplayClock(PlaybackMode mode, double acceleration_factor = 1.0)
        : mode_(mode), factor_(acceleration_factor), first_message_ts_(0), start_time_ns_(0) {}

    void wait_until(uint64_t message_timestamp_ns) {
        if (mode_ == PlaybackMode::MaxThroughput || mode_ == PlaybackMode::SingleStep) {
            return; // No sleep
        }

        if (first_message_ts_ == 0) {
            first_message_ts_ = message_timestamp_ns;
            start_time_ns_ = current_system_ns();
            return;
        }

        if (message_timestamp_ns <= first_message_ts_) {
            return; // Out of order or same timestamp, process immediately
        }

        uint64_t elapsed_message_time = message_timestamp_ns - first_message_ts_;
        
        if (mode_ == PlaybackMode::Accelerated) {
            elapsed_message_time = static_cast<uint64_t>(static_cast<double>(elapsed_message_time) / factor_);
        }

        uint64_t target_system_time = start_time_ns_ + elapsed_message_time;

        // Spin wait (suitable for highly accurate low-latency replay)
        // Note: For long delays, yielding is better. But typically PCAP replays are dense.
        while (current_system_ns() < target_system_time) {
            std::this_thread::yield();
        }
    }

private:
    static uint64_t current_system_ns() {
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
    }

    PlaybackMode mode_;
    double factor_;
    uint64_t first_message_ts_;
    uint64_t start_time_ns_;
};

} // namespace tradecore::replay
