#pragma once

#include "tradecore/lockfree/spsc_queue.hpp"
#include <array>
#include <memory>
#include <cstdint>

namespace tradecore::lockfree {

// Array-of-SPSC queues to simulate a wait-free MPSC queue.
// Eliminates cache-line contention compared to CAS-based MPSC.
template <typename T, std::size_t CapacityPerProducer, std::size_t NumProducers>
class MPSCQueue {
public:
    MPSCQueue() {
        for (std::size_t i = 0; i < NumProducers; ++i) {
            queues_[i] = std::make_unique<SPSCQueue<T, CapacityPerProducer>>();
        }
    }

    ~MPSCQueue() = default;
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    [[nodiscard]] bool push(std::size_t producer_id, const T& item) noexcept {
        TRADECORE_ASSERT(producer_id < NumProducers);
        return queues_[producer_id]->push(item);
    }

    [[nodiscard]] bool push(std::size_t producer_id, T&& item) noexcept {
        TRADECORE_ASSERT(producer_id < NumProducers);
        return queues_[producer_id]->push(std::move(item));
    }

    // Consumer polls queues round-robin. Returns true if an item was popped.
    [[nodiscard]] bool pop(T& item) noexcept {
        for (std::size_t i = 0; i < NumProducers; ++i) {
            std::size_t idx = (next_poll_idx_ + i) % NumProducers;
            if (queues_[idx]->pop(item)) {
                next_poll_idx_ = (idx + 1) % NumProducers;
                return true;
            }
        }
        return false;
    }

private:
    std::array<std::unique_ptr<SPSCQueue<T, CapacityPerProducer>>, NumProducers> queues_;
    std::size_t next_poll_idx_{0};
};

} // namespace tradecore::lockfree
