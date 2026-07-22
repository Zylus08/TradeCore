#pragma once

#include "tradecore/common/assert.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace tradecore::memory {

class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t capacity, void* buffer) noexcept
        : base_(static_cast<std::byte*>(buffer)),
          current_(base_),
          capacity_(capacity) {}

    ~ArenaAllocator() = default;

    // Non-copyable, non-movable
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment) noexcept {
        std::size_t adjustment = get_alignment_adjustment(current_, alignment);
        if (TRADECORE_UNLIKELY(current_ + adjustment + size > base_ + capacity_)) {
            return nullptr; // Out of memory
        }

        std::byte* aligned_ptr = current_ + adjustment;
        current_ = aligned_ptr + size;
        return aligned_ptr;
    }

    template <typename T>
    [[nodiscard]] T* allocate() noexcept {
        return static_cast<T*>(allocate(sizeof(T), alignof(T)));
    }

    void reset() noexcept {
        current_ = base_;
    }

    [[nodiscard]] std::size_t used_bytes() const noexcept {
        return static_cast<std::size_t>(current_ - base_);
    }

private:
    static std::size_t get_alignment_adjustment(const void* ptr, std::size_t alignment) noexcept {
        auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr);
        std::size_t remainder = ptr_val % alignment;
        return (remainder == 0) ? 0 : (alignment - remainder);
    }

    std::byte* const base_;
    std::byte* current_;
    const std::size_t capacity_;
};

} // namespace tradecore::memory
