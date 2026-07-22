#pragma once

#include "tradecore/common/assert.hpp"
#include "tradecore/common/constants.hpp"
#include <array>
#include <cstdint>
#include <new>

namespace tradecore::memory {

template <typename T, std::size_t Capacity>
class ObjectPool {
public:
    static constexpr std::size_t kCapacity = Capacity;
    using Index = uint32_t;

    ObjectPool() noexcept {
        reset();
    }

    ~ObjectPool() {
        // Does not call destructors for active objects on purpose,
        // as pool teardown is assumed to happen at shutdown.
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    [[nodiscard]] T* allocate() noexcept {
        if (TRADECORE_UNLIKELY(free_head_ == kInvalidPoolIndex)) {
            return nullptr;
        }

        const Index idx = free_head_;
        free_head_ = slots_[idx].next_free;
        ++active_count_;

        return new (&slots_[idx].object) T{};
    }

    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) noexcept {
        if (TRADECORE_UNLIKELY(free_head_ == kInvalidPoolIndex)) {
            return nullptr;
        }

        const Index idx = free_head_;
        free_head_ = slots_[idx].next_free;
        ++active_count_;

        return new (&slots_[idx].object) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr) noexcept {
        TRADECORE_ASSERT(ptr != nullptr);
        
        auto* slot_ptr = reinterpret_cast<Slot*>(ptr);
        const auto offset = slot_ptr - slots_.data();
        
        TRADECORE_ASSERT(offset >= 0 && static_cast<std::size_t>(offset) < Capacity);

        ptr->~T();

        const Index idx = static_cast<Index>(offset);
        slots_[idx].next_free = free_head_;
        free_head_ = idx;
        --active_count_;
    }

    void reset() noexcept {
        for (Index i = 0; i < Capacity - 1; ++i) {
            slots_[i].next_free = i + 1;
        }
        slots_[Capacity - 1].next_free = kInvalidPoolIndex;
        free_head_ = 0;
        active_count_ = 0;
    }

    [[nodiscard]] std::size_t active_count() const noexcept {
        return active_count_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return Capacity;
    }

    [[nodiscard]] Index index_of(const T* ptr) const noexcept {
        auto* slot_ptr = reinterpret_cast<const Slot*>(ptr);
        return static_cast<Index>(slot_ptr - slots_.data());
    }

    [[nodiscard]] T* at(Index idx) noexcept {
        TRADECORE_ASSERT(idx < Capacity);
        return &slots_[idx].object;
    }

    [[nodiscard]] const T* at(Index idx) const noexcept {
        TRADECORE_ASSERT(idx < Capacity);
        return &slots_[idx].object;
    }

private:
    union Slot {
        T object;
        Index next_free;

        Slot() {}
        ~Slot() {}
    };

    alignas(kCacheLineSize) std::array<Slot, Capacity> slots_;
    Index free_head_{0};
    std::size_t active_count_{0};
};

} // namespace tradecore::memory
