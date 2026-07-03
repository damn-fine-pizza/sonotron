#pragma once

#include <cstddef>
#include <type_traits>

#include "arrangrr/common/assert.hpp"

// Own non-owning view type instead of std::span: guaranteed available and
// identical on host and freestanding arm builds (DESIGN.md §20, D32).

namespace arrangrr {

template <typename T>
class Span {
 public:
  constexpr Span() noexcept = default;
  constexpr Span(T* data, std::size_t size) noexcept : data_(data), size_(size) {}
  template <std::size_t N>
  constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

  // A Span<const T> is constructible from a Span<T>.
  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_same_v<const U, T>)
  constexpr Span(Span<U> other) noexcept : data_(other.data()), size_(other.size()) {}

  constexpr T* data() const noexcept { return data_; }
  constexpr std::size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr T& operator[](std::size_t i) const noexcept {
    ARR_ASSERT(i < size_);
    return data_[i];
  }

  constexpr T* begin() const noexcept { return data_; }
  constexpr T* end() const noexcept { return data_ + size_; }

  constexpr Span first(std::size_t n) const noexcept {
    ARR_ASSERT(n <= size_);
    return Span(data_, n);
  }
  constexpr Span subspan(std::size_t offset) const noexcept {
    ARR_ASSERT(offset <= size_);
    return Span(data_ + offset, size_ - offset);
  }
  constexpr Span subspan(std::size_t offset, std::size_t n) const noexcept {
    ARR_ASSERT(offset + n <= size_);
    return Span(data_ + offset, n);
  }

 private:
  T* data_ = nullptr;
  std::size_t size_ = 0;
};

}  // namespace arrangrr
