#pragma once

#include <cstddef>
#include <type_traits>

#include "arrangrr/common/assert.hpp"
#include "arrangrr/common/span.hpp"

namespace arrangrr {

// Fixed-capacity vector with inline storage: the bounded workhorse of the
// core (D32: zero dynamic allocation anywhere). T must be default
// constructible and copy assignable; element slots are value-initialized up
// front, which keeps the whole type constexpr-usable and trivially
// serializable for POD T.
template <typename T, std::size_t N>
class StaticVector {
  static_assert(N > 0);
  static_assert(std::is_default_constructible_v<T>);
  static_assert(std::is_copy_assignable_v<T>);

 public:
  constexpr StaticVector() = default;

  static constexpr std::size_t capacity() noexcept { return N; }
  constexpr std::size_t size() const noexcept { return m_size; }
  constexpr bool empty() const noexcept { return m_size == 0; }
  constexpr bool full() const noexcept { return m_size == N; }

  // Returns false (does not trap) when full: overflow is a runtime condition
  // the caller must handle gracefully (DESIGN.md §20 graceful degradation).
  [[nodiscard]] constexpr bool push_back(const T& value) noexcept {
    if (full()) {
      return false;
    }
    m_data[m_size++] = value;
    return true;
  }

  constexpr void pop_back() noexcept {
    ARR_ASSERT(!empty());
    --m_size;
  }

  constexpr void clear() noexcept { m_size = 0; }

  // Removes the element at `i`, shifting the tail left (stable order).
  constexpr void erase(std::size_t i) noexcept {
    ARR_ASSERT(i < m_size);
    for (std::size_t k = i + 1; k < m_size; ++k) {
      m_data[k - 1] = m_data[k];
    }
    --m_size;
  }

  constexpr T& operator[](std::size_t i) noexcept {
    ARR_ASSERT(i < m_size);
    return m_data[i];
  }
  constexpr const T& operator[](std::size_t i) const noexcept {
    ARR_ASSERT(i < m_size);
    return m_data[i];
  }

  constexpr T& front() noexcept { return (*this)[0]; }
  constexpr T& back() noexcept { return (*this)[m_size - 1]; }
  constexpr const T& front() const noexcept { return (*this)[0]; }
  constexpr const T& back() const noexcept { return (*this)[m_size - 1]; }

  constexpr T* begin() noexcept { return m_data; }
  constexpr T* end() noexcept { return m_data + m_size; }
  constexpr const T* begin() const noexcept { return m_data; }
  constexpr const T* end() const noexcept { return m_data + m_size; }

  constexpr Span<T> span() noexcept { return Span<T>(m_data, m_size); }
  constexpr Span<const T> span() const noexcept { return Span<const T>(m_data, m_size); }

 private:
  T m_data[N]{};
  std::size_t m_size = 0;
};

}  // namespace arrangrr
