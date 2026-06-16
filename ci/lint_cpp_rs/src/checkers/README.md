# Rust C++ Linter Checker Layout

The Rust C++ linter keeps a single crate-level dispatch path in `lib.rs` through
`include!` so files are still read once by `MultiCheckerFileProcessor` and shared
with all applicable checkers. The implementation is split into small source
files so edits and reviews no longer happen inside one 7k-line file.

Python checker prior art lives in `ci/lint_cpp/*_checker.py`. The Rust checker
chunks are grouped by related policy area:

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
