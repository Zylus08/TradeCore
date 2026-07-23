#pragma once

#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <atomic>
#include <cstddef>
#include <vector>
#include <memory>
#include <new>
#include <bit>

namespace tradecore::lockfree {

// Wait-free Single-Producer Single-Consumer Queue
// Bounded capacity, must be a power of 2.
template <typename T, std::size_t Capacity>
class SPSCQueue {
    static_assert(std::has_single_bit(Capacity), "Capacity must be a power of 2");

public:
    SPSCQueue() : buffer_(Capacity) {}

    ~SPSCQueue() = default;

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    [[nodiscard]] bool push(const T& item) noexcept {
        const auto current_tail = producer_.tail.load(std::memory_order_relaxed);
        const auto next_tail = current_tail + 1;

        if (TRADECORE_UNLIKELY(next_tail - producer_.head_cached > Capacity)) {
            // Cache is invalid or queue is actually full, load actual head
            producer_.head_cached = consumer_.head.load(std::memory_order_acquire);
            if (next_tail - producer_.head_cached > Capacity) {
                return false; // Queue full
            }
        }

        buffer_[current_tail & (Capacity - 1)] = item;
        producer_.tail.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool push(T&& item) noexcept {
        const auto current_tail = producer_.tail.load(std::memory_order_relaxed);
        const auto next_tail = current_tail + 1;

        if (TRADECORE_UNLIKELY(next_tail - producer_.head_cached > Capacity)) {
            producer_.head_cached = consumer_.head.load(std::memory_order_acquire);
            if (next_tail - producer_.head_cached > Capacity) {
                return false;
            }
        }

        buffer_[current_tail & (Capacity - 1)] = std::move(item);
        producer_.tail.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(T& item) noexcept {
        const auto current_head = consumer_.head.load(std::memory_order_relaxed);

        if (TRADECORE_UNLIKELY(current_head == consumer_.tail_cached)) {
            // Cache is invalid or queue is actually empty, load actual tail
            consumer_.tail_cached = producer_.tail.load(std::memory_order_acquire);
            if (current_head == consumer_.tail_cached) {
                return false; // Queue empty
            }
        }

        item = std::move(buffer_[current_head & (Capacity - 1)]);
        consumer_.head.store(current_head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const auto t = producer_.tail.load(std::memory_order_acquire);
        const auto h = consumer_.head.load(std::memory_order_acquire);
        return static_cast<std::size_t>(t - h);
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    // To prevent false sharing, we strictly segregate the producer's mutation state 
    // from the consumer's mutation state into distinct structs aligned to the hardware 
    // destructive interference size.

    struct alignas(kHardwareDestructiveInterferenceSize) ProducerState {
        std::atomic<std::size_t> tail{0};
        std::size_t head_cached{0};
    };

    struct alignas(kHardwareDestructiveInterferenceSize) ConsumerState {
        std::atomic<std::size_t> head{0};
        std::size_t tail_cached{0};
    };

    ProducerState producer_;
    ConsumerState consumer_;

    alignas(kHardwareDestructiveInterferenceSize) std::vector<T> buffer_;
};

} // namespace tradecore::lockfree
