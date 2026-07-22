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
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto next_tail = current_tail + 1;

        if (TRADECORE_UNLIKELY(next_tail - head_cached_ > Capacity)) {
            // Cache is invalid or queue is actually full, load actual head
            head_cached_ = head_.load(std::memory_order_acquire);
            if (next_tail - head_cached_ > Capacity) {
                return false; // Queue full
            }
        }

        buffer_[current_tail & (Capacity - 1)] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool push(T&& item) noexcept {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        const auto next_tail = current_tail + 1;

        if (TRADECORE_UNLIKELY(next_tail - head_cached_ > Capacity)) {
            head_cached_ = head_.load(std::memory_order_acquire);
            if (next_tail - head_cached_ > Capacity) {
                return false;
            }
        }

        buffer_[current_tail & (Capacity - 1)] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(T& item) noexcept {
        const auto current_head = head_.load(std::memory_order_relaxed);

        if (TRADECORE_UNLIKELY(current_head == tail_cached_)) {
            // Cache is invalid or queue is actually empty, load actual tail
            tail_cached_ = tail_.load(std::memory_order_acquire);
            if (current_head == tail_cached_) {
                return false; // Queue empty
            }
        }

        item = std::move(buffer_[current_head & (Capacity - 1)]);
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const auto t = tail_.load(std::memory_order_acquire);
        const auto h = head_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(t - h);
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

private:
    // Prevent false sharing by putting head and tail on different cache lines
    alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};
    alignas(kCacheLineSize) std::size_t head_cached_{0};

    alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
    alignas(kCacheLineSize) std::size_t tail_cached_{0};

    // Buffer can be large, we use std::vector but allocate it upfront
    alignas(kCacheLineSize) std::vector<T> buffer_;
};

} // namespace tradecore::lockfree
