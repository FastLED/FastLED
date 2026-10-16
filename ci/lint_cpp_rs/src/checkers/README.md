# Rust C++ Linter Checker Layout

The Rust C++ linter keeps a single crate-level dispatch path in `lib.rs` through
`include!` so files are still read once by `MultiCheckerFileProcessor` and shared
with all applicable checkers. The implementation is split into small source
files so edits and reviews no longer happen inside one 7k-line file.

As of FastLED #3288 the Rust crate is the default lint path; the 60 ported
Python checkers under `ci/lint_cpp/*_checker.py` have been retired (see git
history for prior art). New checkers go here. The Rust checker chunks are
grouped by related policy area:

- `basic.rs`: serial printf, namespace use in examples, banned macros,
  allocation, static-in-header, include paths, and builtin memcpy checks.
- `preprocessor.rs`: weak attributes, banned defines/namespaces, `.cpp`
  include rules, `.cpp.hpp` include rules, implementation include rules,
  asm-js placement, reinterpret casts, pragma once, and ESP ROM printf.
- `runtime.rs`: sleep/thread/span/relative-include/FastLED-header/Arduino macro
  checks plus attribute, `FL_IS_*`, and numeric limit macro checks.
- `style.rs`: platform/raw pragma, raw noexcept, singleton, `std::`,
  example serial, namespace-after-include, and namespace declaration checks.
- `platform_trampoline.rs`: namespace, platform include, trampoline, SIMD,
  `.cpp.hpp` header-pair, `_is_` header, IWYU pragma, and member-style checks.
- `types_and_tests.rs`: bare using, ctype, stdint, subdir namespace, unit test,
  and header-existence checks.
- `test_structure.rs`: test include-path, test path-structure,
  test aggregation, banned header, and namespace include checks.
- `platform_policy.rs`: native platform define, noexcept special member,
  enum-class, platform namespace, and logging-in-IRAM checks.
- `container_ptr.rs`: raw pointers into `fl::` containers (FastLED#3287) --
  hard-fail on non-contiguous containers, warn-only on contiguous ones.
- `libc_mirror.rs`: any function in `namespace fl` (or a sub-namespace) with a
  libc name must be a `template <typename FlLibcTag = void>` +
  `FASTLED_FORCE_INLINE` wrapper, so unqualified libc calls on fl types stay
  unambiguous (FastLED#4834). Hidden friends count; arithmetic-only libc
  names (`abs`, `div`, ...) only need to be templates; variables, types and
  unscoped enumerators directly in `fl` may not use a libc name; only
  `fl::libc_impl` is exempt and it may not define types (FastLED#4838). A
  function-like macro that starts a declaration in `fl` may not take a libc
  name as an argument, since its expansion is invisible to the scan
  (FastLED#4850). The
  name tables live in `lint_core/libc_reserved_names.rs`.
- `libc_backstop.rs`: structural pass. Every `FlLibcTag` wrapper under `src/`
  must be called unqualified in `tests/fl/stl/adl_barrier.cpp`'s
  `libc_every_using_fl` block, and every wrapper with a `void*` parameter also
  in `libc_every_adl`; calls to names that are no longer wrappers fail
  (FastLED#4838).
- `adl_barrier.rs`: structural pass. No function may be declared directly in
  `fl` with the name of a function in `fl::adl_barrier`, and every barrier
  function must be called as `fl::name` in the "every adl_barrier name is
  reachable as fl::name" case of `tests/fl/stl/adl_barrier.cpp` (FastLED#4851).
