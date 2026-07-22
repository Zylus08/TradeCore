#pragma once

#include "tradecore/orderbook/order.hpp"
#include "tradecore/memory/object_pool.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <cstdint>

namespace tradecore::orderbook {

// Typed wrapper around ObjectPool specialized for Order.
// Provides semantic index-based access and bounds checking.
template <std::size_t Capacity>
class OrderPool {
public:
    using Index = uint32_t;
    static constexpr Index kInvalid = kInvalidPoolIndex;

    OrderPool() noexcept = default;

    [[nodiscard]] Order* allocate() noexcept {
        return pool_.allocate();
    }

    template <typename... Args>
    [[nodiscard]] Order* allocate(Args&&... args) noexcept {
        return pool_.allocate(std::forward<Args>(args)...);
    }

    void deallocate(Order* ptr) noexcept {
        pool_.deallocate(ptr);
    }

    [[nodiscard]] Order* at(Index idx) noexcept {
        TRADECORE_ASSERT(idx < Capacity);
        return pool_.at(idx);
    }

    [[nodiscard]] const Order* at(Index idx) const noexcept {
        TRADECORE_ASSERT(idx < Capacity);
        return pool_.at(idx);
    }

    [[nodiscard]] Index index_of(const Order* ptr) const noexcept {
        return pool_.index_of(ptr);
    }

    [[nodiscard]] std::size_t active_count() const noexcept {
        return pool_.active_count();
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    void reset() noexcept { pool_.reset(); }

private:
    memory::ObjectPool<Order, Capacity> pool_;
};

} // namespace tradecore::orderbook
