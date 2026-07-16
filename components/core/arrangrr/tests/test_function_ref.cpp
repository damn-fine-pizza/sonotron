// Unit tests for FunctionRef (components/core/arrangrr/common/function_ref.hpp):
// the non-owning, two-word callable reference used across the engine ABI
// boundary (EventSink and friends). Pure, freestanding, no Engine.
//
// Phase 6 Theme 1b (Torquato QA, coverage-gate restoration): this file was
// entirely missing before -- function_ref.hpp sat at 54% line coverage
// under the unit gate despite being a pure, trivially unit-testable type.
// Note: FunctionRef has NO default constructor and NO null/empty state by
// design (a non-owning reference must always bind to something) -- "empty"
// coverage here means a zero-argument signature and a non-capturing
// (stateless) callable, as opposed to a capturing/stateful one, not a null
// FunctionRef (which cannot exist).

#include "arrangrr/common/function_ref.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

int add_one(int x) { return x + 1; }

void test_call_forwards_arguments_and_return_value() {
  const auto lambda = [](int a, int b) { return a * 10 + b; };
  FunctionRef<int(int, int)> ref = lambda;
  CHECK(ref(3, 4) == 34);
  CHECK(ref(0, 0) == 0);
}

void test_call_with_zero_argument_signature() {
  int calls = 0;
  const auto lambda = [&calls]() {
    ++calls;
    return 7;
  };
  FunctionRef<int()> ref = lambda;
  CHECK(ref() == 7);
  CHECK(ref() == 7);
  CHECK(calls == 2);
}

void test_binds_a_free_function_pointer() {
  // A free function's address is a valid callable lvalue (an addressable
  // function-pointer object), not just lambdas/functors.
  auto* fn = &add_one;
  FunctionRef<int(int)> ref = fn;
  CHECK(ref(41) == 42);
}

void test_non_capturing_stateless_lambda() {
  const auto lambda = [](int x) { return x * x; };
  FunctionRef<int(int)> ref = lambda;
  CHECK(ref(5) == 25);
  CHECK(ref(6) == 36);  // stateless: repeated calls are independent, no drift
}

void test_reference_semantics_no_copy_mutation_is_observed_by_the_caller() {
  // The referenced callable is NEVER copied: mutating captured state through
  // the SAME FunctionRef instance must be visible to the ORIGINAL object the
  // caller still holds -- the defining property of a non-owning reference
  // (as opposed to std::function's owning, copying semantics).
  int counter = 0;
  auto counting = [&counter](int delta) { counter += delta; };
  FunctionRef<void(int)> ref = counting;
  ref(1);
  ref(2);
  ref(3);
  CHECK(counter == 6);  // observed directly on the caller's own `counter`
}

void test_void_return_signature() {
  int last_seen = -1;
  auto sink = [&last_seen](int v) { last_seen = v; };
  FunctionRef<void(int)> ref = sink;
  ref(99);
  CHECK(last_seen == 99);
}

// Rebinding: the SAME declared FunctionRef variable (or, equivalently, a
// call-site parameter across two different call sites) can be bound to two
// DIFFERENT underlying callables in sequence -- there is no persistent
// identity beyond the two words it stores, so reassignment is a plain
// pointer-pair overwrite (FunctionRef's implicitly-generated copy-assignment
// operator, since it has no user-declared special members).
void test_rebind_to_a_different_callable() {
  int a_calls = 0;
  int b_calls = 0;
  auto callable_a = [&a_calls](int x) {
    ++a_calls;
    return x + 100;
  };
  auto callable_b = [&b_calls](int x) {
    ++b_calls;
    return x + 200;
  };
  FunctionRef<int(int)> ref = callable_a;
  CHECK(ref(1) == 101);
  ref = callable_b;  // rebind: now forwards to callable_b instead
  CHECK(ref(1) == 201);
  CHECK(a_calls == 1);
  CHECK(b_calls == 1);
}

// Mirrors the actual production use pattern (EventSink-shaped): a helper
// function taking a FunctionRef BY VALUE as a parameter and invoking it,
// exercising the pass-through-a-function-boundary path the whole engine ABI
// relies on.
void forward_to_sink(FunctionRef<void(int, int)> sink) {
  sink(1, 2);
  sink(3, 4);
}

void test_passed_as_a_function_parameter() {
  int sum = 0;
  auto accumulate = [&sum](int a, int b) { sum += a + b; };
  forward_to_sink(accumulate);
  CHECK(sum == 10);  // (1+2) + (3+4)
}

}  // namespace

int main() {
  test_call_forwards_arguments_and_return_value();
  test_call_with_zero_argument_signature();
  test_binds_a_free_function_pointer();
  test_non_capturing_stateless_lambda();
  test_reference_semantics_no_copy_mutation_is_observed_by_the_caller();
  test_void_return_signature();
  test_rebind_to_a_different_callable();
  test_passed_as_a_function_parameter();
  return arrangrr::test::failures();
}
