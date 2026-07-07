#pragma once

#include <memory>
#include <type_traits>

// Non-owning callable reference: two words, no allocation, no exceptions —
// the D32-compatible way to pass a sink across the engine ABI boundary
// (monomorphic API, one instantiation in flash). The referenced callable
// must outlive the call — engine sinks are lvalues owned by the caller.

namespace arrangrr {

template <typename Sig>
class FunctionRef;

template <typename R, typename... Args>
class FunctionRef<R(Args...)> {
 public:
  template <typename F>
    requires(!std::is_same_v<std::remove_cvref_t<F>, FunctionRef> &&
             std::is_invocable_r_v<R, F&, Args...>)
  constexpr FunctionRef(F&& f) noexcept  // NOLINT: implicit by design
      : m_obj(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),
        m_fn(+[](void* obj, Args... args) -> R {
          return (*static_cast<std::remove_reference_t<F>*>(obj))(static_cast<Args&&>(args)...);
        }) {}

  constexpr R operator()(Args... args) const { return m_fn(m_obj, static_cast<Args&&>(args)...); }

 private:
  void* m_obj;
  R (*m_fn)(void*, Args...);
};

}  // namespace arrangrr
