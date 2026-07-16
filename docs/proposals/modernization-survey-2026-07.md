# Sonotron / arrangrr — Modernization, Performance & Allocation Survey

**Status:** proposal / survey findings — 2026-07-16
**Author:** paganini-modernization-surveyor (whole-codebase survey)
**Scope:** `components/core/**` (freestanding regime), `components/platform/**` + `apps/**` (host regime)

The codebase is genuinely disciplined: the core is heap-clean, uses custom
`Span`/`StaticVector`/`Result` primitives, leans hard on `constexpr`, and the
runtime pipeline is already modern (compile-time recursive composition,
`if constexpr` capability probing, guaranteed copy elision). The findings below
are refinements on an already-strong base, not rescue work.

## CRITICAL — core-regime allocation violations

**None found.** A full sweep of `components/core/**` non-test sources for
`new`/`delete`/`malloc`/`std::vector`/`std::string`/`std::map`/`std::function`/
`make_unique`/`make_shared`/`std::deque`/`std::list` returned only the English
word "new" inside comments. The no-heap freestanding law (D32) is intact.
Dynamic-container types appear only in `components/platform/**` and `apps/**`
(host regime, where heap is permitted).

---

## (A) Prioritized improvements

**1. Collapse the duplicated `kRoleCount = 10` single-source-of-truth violation.**
`components/core/arrangrr/include/arrangrr/arranger/voicing.hpp:130` and
`components/core/arrangrr/include/arrangrr/arranger/arranger.hpp:679` each
independently define `kRoleCount = 10`. Two hand-copied constants for the same
TrackRole cardinality will drift the day a role is added. Define it once (next
to the `TrackRole` enum) and reference it from both. *Readability /
maintainability; zero codegen change.*

**2. Replace hand-maintained enum-cardinality constants with a derived/sentinel count.**
`kInsertTypeCount = 6` (`fx/insert_chain.hpp:61`), `kArpDirectionCount = 6`,
`kArpRateCount = 4`, `kArpFieldCount = 7` (`arp/arpeggiator.hpp:32/40/76`),
`kGrooveFieldCount = 7` (`arranger/groove.hpp:41`), `kSectionTypeCount = 13`,
`kMotifTransformCount = 4` (`arranger/style_model.hpp:35/118`), `kChordModeCount
= 3` (`chord/chord_engine.hpp:29`). Each is a magic integer that must be bumped
in lockstep with its enum. Prefer a trailing `kCount` enumerator (`… kArp,
kCount };` then `constexpr auto kInsertTypeCount =
std::to_underlying(InsertType::kCount);`). Where a wire/ABI byte pins the count
deliberately, keep the literal but add a `static_assert(kFooCount ==
std::to_underlying(Foo::kCount))` to catch drift. *Modern C++ +
maintainability; regime-safe (all constexpr).*

**3. Adopt `std::to_underlying` (C++23) for the ~33 pure enum→underlying casts.**
e.g. `fx/insert_chain.hpp:81` `static_cast<std::uint8_t>(VelocityProcMode::kScale)`,
`:122` `ArpRate::kSixteenth`, `:123` `ArpDirection::kUp`. `std::to_underlying`
states the intent and is immune to the underlying type changing. Apply it *only*
to pure enum-value casts — leave bit-masking casts (`common/midi/message.hpp:94/98`,
`engine.cpp` `cmd.a & 0xFF`) as `static_cast`, they are int arithmetic. C++23,
universal across all six target compilers, no gating. *Readability + modern C++.*

**4. Establish the feature-test-macro dual-branch pattern the house policy already sanctions — it currently has zero instances.**
A grep for `__cpp_` across all product code returns nothing, yet the doctrine
explicitly accepts `#ifdef __cpp_…` gated dual branches for C++26 features with
a fallback. The policy exists but has never been exercised. Pilot it with the
lowest-risk high-value case (item 5); a `components/core/arrangrr/common/`
compatibility shim header is the natural home. *Modern C++ doctrine; unblocks
safe C++26 adoption.*

**5. Alias `StaticVector<T,N>` onto `std::inplace_vector<T,N>` (C++26) behind `__cpp_lib_inplace_vector`.**
`components/core/arrangrr/include/arrangrr/common/static_vector.hpp` is a
hand-rolled fixed-capacity inline-storage vector — exactly `std::inplace_vector`.
Its distinguishing contract is the non-throwing graceful `push_back` returning
`bool` (D32/§20), which maps to `inplace_vector::try_push_back`. When
`__cpp_lib_inplace_vector` is defined (and freestanding-clean on arm libstdc++),
provide the class as a thin wrapper; otherwise keep the current implementation
verbatim. Ideal first feature-macro branch — behaviour-identical, well-tested.
Keep the custom fallback indefinitely. *Modern C++ (gated); allocation
unchanged.*

**6. Host: `split_ws` heap-allocates a fresh `std::vector<std::string_view>` on every command parse.**
`apps/gui-sonotron/src/in_process_brain_session.cpp:247` returns
`std::vector<std::string_view>`, and the pattern recurs across the translate
path. A shell command line has a small bounded token count; this is
stack-eligible. Reuse the core's header-only `StaticVector<std::string_view,
kMaxTokens>` (host-includable) to drop the per-keystroke allocation. Host
regime, not a violation, but stack/fixed-capacity is preferred when it doesn't
hurt clarity — and here it improves it (bounded by construction).
*Allocation; readability neutral-to-positive.*

**7. Host non-owning callbacks: prefer `std::function_ref` (C++26, gated) over `std::function` where no ownership is needed.**
`shell.hpp:57–71` (`EventSink`, `PortHook`, `PanelHook`, `PrintHook`,
`WidthProvider`, `HeightProvider`), `console.hpp:82`, `tui_client.hpp:36`,
`uds_server.hpp:61/68`. Where the callable outlives the call site as a borrowed
reference, `std::function_ref` is allocation-free and non-owning. **Must be
feature-macro-gated** (`__cpp_lib_function_ref`): reliable on GCC 15+/Clang 19+
but not dependable on Apple Clang or MSVC yet — keep `std::function` as fallback.
Audit each site for genuine ownership first. *Allocation (host, gated); care.*

**8. Confirm-and-keep: the custom `Span`/`Result` "not guaranteed on freestanding" justifications deserve a re-verification note.**
`common/span.hpp:8` and `common/result.hpp:7` justify bespoke types because
`<span>`/`<expected>` were "not guaranteed" freestanding. As of the pinned floor
(arm-none-eabi GCC 15), `std::span` is in the C++23 freestanding subset and
`<expected>` freestanding support has landed in libstdc++. **Not a change to
make now** — the custom types are zero-cost and give the exact graceful contract
— but add a one-line "re-audit at GCC-16-arm floor" note so the decision is
revisited deliberately. *Documentation accuracy; no code change.*

**9. `StaticVector::push_back(const T&)` has no move/emplace path.**
`static_vector.hpp:32`. Harmless for the POD element types held today, but a
non-trivial element would force a copy. A `push_back(T&&)` overload and/or a
variadic `emplace_back` returning `bool` (mirroring the graceful contract)
future-proofs it at zero cost. Low priority (every current `T` is trivially
copyable). *Modern C++; low urgency.*

**10. Extend `[[nodiscard]]` coverage to the graceful-failure returns.**
The bounded-drop pattern (`push_back` returning `bool`, `Result`, timeline add
returning `-1`) is the core's safety net. `StaticVector::push_back` is
`[[nodiscard]]`; survey the sibling graceful-failure returns (`timeline.hpp:81`
"returns the new track index, or -1 when the pool is full", `clip_matrix`
registration) and mark those `[[nodiscard]]` too, so a dropped-because-full
return can never be silently ignored. *Correctness-adjacent; C++17, universal.*

---

## (B) Toolchain + standard report

### Real compilers probed (2026-07-16, this box)

| Toolchain | Version | Role |
|---|---|---|
| `g++` | **GCC 16.1.1** (Red Hat 16.1.1-2) | host |
| `clang++` | **Clang 22.1.8** (Fedora) | host (clang-tidy / second opinion) |
| `x86_64-w64-mingw32-g++` | **GCC 16.1.1** (Fedora MinGW) | future Windows cross (installed, verified) |
| `arm-none-eabi-g++` | **GCC 15.2.0** (Fedora 15.2.0-4) | firmware (STM32H743, Cortex-M7) |
| `cmake` | 4.2.1 | build driver |

Project dialect (top `CMakeLists.txt:11-14`): `CMAKE_CXX_STANDARD 26`,
`CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`,
`CMAKE_CXX_SCAN_FOR_MODULES OFF`. ARM toolchain
(`cmake/toolchains/arm-cortex-m7.cmake`) pins arm-none-eabi-g++ ≥ 14; the box
has 15.2.0. The binding embedded constraint is **arm-none-eabi-GCC 15**; the two
future CI cross targets (GitHub `macos-latest` = Apple Clang, `windows-latest` =
MSVC) are the binding desktop constraints — both trail mainline on C++26.

### Newest standard usable as the intersection across ALL six

**C++23 is the safe, fully-portable baseline for unconditional feature use.** All
six accept the `-std=c++26` dialect flag (so keeping `CMAKE_CXX_STANDARD 26` is
correct and buildable today on the three real compilers), but **C++26 feature
*use* must be opt-in per feature, gated by `__cpp_*` feature-test macros**,
because Apple Clang and MSVC coverage is uneven.

**Safe across ALL desktop hosts WITHOUT gating (C++23):** `std::to_underlying`,
`if consteval`, `[[assume]]`, `std::span`, `std::expected`, `std::print`/
`std::format` (host only), `constexpr` `std::` algorithms, `std::byteswap`,
multidimensional `operator[]`, `auto(x)`. These underpin improvements 1, 3, 8, 10.

**C++26 features that NEED `__cpp_…`-gated dual branches (fallback required):**
`std::inplace_vector` / `__cpp_lib_inplace_vector` (item 5), `std::function_ref`
/ `__cpp_lib_function_ref` (item 7), `std::execution` / senders, pack indexing
(`__cpp_pack_indexing`), `#embed` (`__has_embed`), contracts. **Reflection
(P2996): NOT portable (experimental/EDG-and-Clang-fork only) — do not adopt.**
Gate every one against the narrowest target (Apple Clang desktop, arm-GCC-15
firmware) and keep the C++23 fallback live.

**Net recommendation:** keep `-std=c++26` as the dialect, treat C++23 as the
unconditional feature floor, and introduce C++26 features only through
`__cpp_*`-gated branches with a C++23 fallback — starting with the two
lowest-risk, highest-value ones (`inplace_vector`, `function_ref`).

### Suggested implementation order

1. **Pure C++23 wins, no gating, no ABI risk:** #1 (`kRoleCount` unify), #3
   (`std::to_underlying`), #6 (`split_ws` stack tokens). Start here.
2. **Cheap correctness:** #10 (`[[nodiscard]]`), #8 (stale-rationale note), #2
   (enum `kCount` + `static_assert`).
3. **Establish the gating idiom:** #4, then the gated modernizations #5
   (`inplace_vector`) and #7 (`function_ref`).
4. #9 last (low urgency).

Sources: [Clang C++ Status](https://clang.llvm.org/cxx_status.html),
[Apple Xcode C++ support](https://developer.apple.com/xcode/cpp/),
[MSVC conformance](https://learn.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance),
[VS 2026 C++ what's new](https://devblogs.microsoft.com/cppblog/whats-new-for-cpp-developers-in-visual-studio-2026-version-18-0/).
