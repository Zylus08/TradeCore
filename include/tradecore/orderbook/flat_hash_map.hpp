#pragma once

#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <cstdint>
#include <cstddef>
#include <bit>
#include <vector>

namespace tradecore::orderbook {

// Open-addressed, linear-probing flat hash map.
// Keys: uint64_t (OrderId). Values: uint32_t (pool index).
// Power-of-2 capacity => bitwise AND modulo, no division on hot path.
// Zero heap allocations after construction.
class FlatHashMap {
public:
    static constexpr uint32_t kEmpty = kInvalidPoolIndex; // sentinel for unused slots
    static constexpr float kMaxLoadFactor = 0.75f;

    explicit FlatHashMap(std::size_t capacity)
        : capacity_(next_power_of_two(capacity)),
          mask_(capacity_ - 1),
          keys_(capacity_, kEmptyKey),
          values_(capacity_, kEmpty),
          size_(0)
    {}

    // Insert or update. Returns false if table is full.
    [[nodiscard]] bool insert(uint64_t key, uint32_t value) noexcept {
        TRADECORE_ASSERT(key != kEmptyKey);

        if (TRADECORE_UNLIKELY(static_cast<float>(size_ + 1) / static_cast<float>(capacity_)
                               >= kMaxLoadFactor)) {
            return false; // Cannot resize on hot path — caller must pre-size
        }

        std::size_t idx = hash(key) & mask_;
        while (keys_[idx] != kEmptyKey && keys_[idx] != key) {
            idx = (idx + 1) & mask_;
        }

        if (keys_[idx] == kEmptyKey) { ++size_; }
        keys_[idx]   = key;
        values_[idx] = value;
        return true;
    }

    // Lookup. Returns kEmpty if not found.
    [[nodiscard]] uint32_t find(uint64_t key) const noexcept {
        TRADECORE_ASSERT(key != kEmptyKey);
        std::size_t idx = hash(key) & mask_;
        while (keys_[idx] != kEmptyKey) {
            if (keys_[idx] == key) return values_[idx];
            idx = (idx + 1) & mask_;
        }
        return kEmpty;
    }

    // Erase. Returns true if key was present.
    [[nodiscard]] bool erase(uint64_t key) noexcept {
        TRADECORE_ASSERT(key != kEmptyKey);
        std::size_t idx = hash(key) & mask_;
        while (keys_[idx] != kEmptyKey) {
            if (keys_[idx] == key) {
                // Backward-shift deletion to maintain probe chains
                keys_[idx]   = kEmptyKey;
                values_[idx] = kEmpty;
                --size_;
                rehash_from(idx);
                return true;
            }
            idx = (idx + 1) & mask_;
        }
        return false;
    }

    [[nodiscard]] bool contains(uint64_t key) const noexcept {
        return find(key) != kEmpty;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    void clear() noexcept {
        std::fill(keys_.begin(), keys_.end(), kEmptyKey);
        std::fill(values_.begin(), values_.end(), kEmpty);
        size_ = 0;
    }

private:
    static constexpr uint64_t kEmptyKey = 0; // OrderId 0 is never valid

    static std::size_t next_power_of_two(std::size_t n) noexcept {
        if (n == 0) return 1;
        return std::size_t{1} << (std::bit_width(n - 1));
    }

    static std::size_t hash(uint64_t key) noexcept {
        // Fibonacci hashing: multiply by 2^64 / phi, then shift down
        return static_cast<std::size_t>(key * 11400714819323198485ULL);
    }

    // After erasing a slot, re-insert all entries in the same probe chain
    // to avoid leaving "holes" that break subsequent lookups.
    void rehash_from(std::size_t erased_idx) noexcept {
        std::size_t idx = (erased_idx + 1) & mask_;
        while (keys_[idx] != kEmptyKey) {
            uint64_t displaced_key   = keys_[idx];
            uint32_t displaced_value = values_[idx];
            keys_[idx]   = kEmptyKey;
            values_[idx] = kEmpty;
            --size_;
            insert(displaced_key, displaced_value);
            idx = (idx + 1) & mask_;
        }
    }

    std::size_t        capacity_;
    std::size_t        mask_;
    std::vector<uint64_t> keys_;
    std::vector<uint32_t> values_;
    std::size_t        size_;
};

} // namespace tradecore::orderbook
