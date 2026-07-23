#pragma once

#include "tradecore/common/assert.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <span>

namespace tradecore::common {

template <typename T, std::size_t Capacity>
class StaticVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    StaticVector() = default;

    void push_back(const T& val) noexcept {
        TRADECORE_ASSERT(size_ < Capacity);
        data_[size_++] = val;
    }

    void push_back(T&& val) noexcept {
        TRADECORE_ASSERT(size_ < Capacity);
        data_[size_++] = std::move(val);
    }

    [[nodiscard]] size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr size_type capacity() const noexcept { return Capacity; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    void clear() noexcept { size_ = 0; }

    [[nodiscard]] reference operator[](size_type idx) noexcept {
        TRADECORE_ASSERT(idx < size_);
        return data_[idx];
    }

    [[nodiscard]] const_reference operator[](size_type idx) const noexcept {
        TRADECORE_ASSERT(idx < size_);
        return data_[idx];
    }

    [[nodiscard]] T* begin() noexcept { return data_.data(); }
    [[nodiscard]] const T* begin() const noexcept { return data_.data(); }
    [[nodiscard]] T* end() noexcept { return data_.data() + size_; }
    [[nodiscard]] const T* end() const noexcept { return data_.data() + size_; }

    [[nodiscard]] std::span<T> as_span() noexcept {
        return std::span<T>(data_.data(), size_);
    }

    [[nodiscard]] std::span<const T> as_span() const noexcept {
        return std::span<const T>(data_.data(), size_);
    }

private:
    std::array<T, Capacity> data_{};
    size_type size_{0};
};

} // namespace tradecore::common
