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

  constexpr bool is_ok() const noexcept { return ok_; }
  constexpr explicit operator bool() const noexcept { return ok_; }

  constexpr T& value() noexcept {
    ARR_ASSERT(ok_);
    return val_;
  }
  constexpr const T& value() const noexcept {
    ARR_ASSERT(ok_);
    return val_;
  }
  constexpr E& error() noexcept {
    ARR_ASSERT(!ok_);
    return err_;
  }
  constexpr const E& error() const noexcept {
    ARR_ASSERT(!ok_);
    return err_;
  }

  constexpr T value_or(T fallback) const noexcept { return ok_ ? val_ : fallback; }

 private:
  struct OkTag {};
  struct ErrTag {};
  constexpr Result(T v, OkTag) noexcept : val_(v), ok_(true) {}
  constexpr Result(E e, ErrTag) noexcept : err_(e), ok_(false) {}

  union {
    T val_;
    E err_;
  };
  bool ok_;
};

// Result<void, E>: success carries no payload.
template <typename E>
class [[nodiscard]] Status {
  static_assert(std::is_trivially_destructible_v<E>);

 public:
  static constexpr Status ok() noexcept { return Status(true, E{}); }
  static constexpr Status err(E error) noexcept { return Status(false, error); }

  constexpr bool is_ok() const noexcept { return ok_; }
  constexpr explicit operator bool() const noexcept { return ok_; }

  constexpr const E& error() const noexcept {
    ARR_ASSERT(!ok_);
    return err_;
  }

 private:
  constexpr Status(bool ok, E e) noexcept : err_(e), ok_(ok) {}
  E err_;
  bool ok_;
};

}  // namespace arrangrr
