#pragma once

#include <type_traits>

#include "arrangrr/common/assert.hpp"

// Own Result<T, E> instead of std::expected: <expected> is not guaranteed on
// freestanding arm libstdc++ (DESIGN.md §20, Francesco M9). Restricted to
// trivially destructible payloads — the core is POD-first (D32).

namespace arrangrr {

template <typename T, typename E>
class [[nodiscard]] Result {
  static_assert(std::is_trivially_destructible_v<T>);
  static_assert(std::is_trivially_destructible_v<E>);

 public:
  static constexpr Result ok(T value) noexcept { return Result(value, OkTag{}); }
  static constexpr Result err(E error) noexcept { return Result(error, ErrTag{}); }

  constexpr bool is_ok() const noexcept { return m_ok; }
  constexpr explicit operator bool() const noexcept { return m_ok; }

  constexpr T& value() noexcept {
    ARR_ASSERT(m_ok);
    return val_;
  }
  constexpr const T& value() const noexcept {
    ARR_ASSERT(m_ok);
    return val_;
  }
  constexpr E& error() noexcept {
    ARR_ASSERT(!m_ok);
    return err_;
  }
  constexpr const E& error() const noexcept {
    ARR_ASSERT(!m_ok);
    return err_;
  }

  constexpr T value_or(T fallback) const noexcept { return m_ok ? val_ : fallback; }

 private:
  struct OkTag {};
  struct ErrTag {};
  constexpr Result(T v, OkTag) noexcept : val_(v), m_ok(true) {}
  constexpr Result(E e, ErrTag) noexcept : err_(e), m_ok(false) {}

  union {
    T val_;
    E err_;
  };
  bool m_ok;
};

// Result<void, E>: success carries no payload.
template <typename E>
class [[nodiscard]] Status {
  static_assert(std::is_trivially_destructible_v<E>);

 public:
  static constexpr Status ok() noexcept { return Status(true, E{}); }
  static constexpr Status err(E error) noexcept { return Status(false, error); }

  constexpr bool is_ok() const noexcept { return m_ok; }
  constexpr explicit operator bool() const noexcept { return m_ok; }

  constexpr const E& error() const noexcept {
    ARR_ASSERT(!m_ok);
    return m_err;
  }

 private:
  constexpr Status(bool ok, E e) noexcept : m_err(e), m_ok(ok) {}
  E m_err;
  bool m_ok;
};

}  // namespace arrangrr
