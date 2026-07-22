#pragma once

#include "tradecore/orderbook/price_level.hpp"
#include "tradecore/memory/object_pool.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <cstdint>

namespace tradecore::orderbook {

// Typed wrapper around ObjectPool specialized for PriceLevel.
template <std::size_t Capacity>
class PriceLevelPool {
public:
    using Index = uint32_t;
    static constexpr Index kInvalid = kInvalidPoolIndex;

    PriceLevelPool() noexcept = default;

    [[nodiscard]] PriceLevel* allocate() noexcept {
        return pool_.allocate();
    }

    void deallocate(PriceLevel* ptr) noexcept {
        pool_.deallocate(ptr);
    }

    [[nodiscard]] PriceLevel* at(Index idx) noexcept {
        TRADECORE_ASSERT(idx < Capacity);
        return pool_.at(idx);
    }

    [[nodiscard]] const PriceLevel* at(Index idx) const noexcept {
        TRADECORE_ASSERT(idx < Capacity);
        return pool_.at(idx);
    }

    [[nodiscard]] Index index_of(const PriceLevel* ptr) const noexcept {
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
    memory::ObjectPool<PriceLevel, Capacity> pool_;
};

} // namespace tradecore::orderbook
