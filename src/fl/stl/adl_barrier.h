#pragma once

/// @file adl_barrier.h
/// Keeps fl's generic templates out of argument-dependent lookup (ADL).
///
/// Every fl type (CRGB, CHSV, palettes, containers, ...) lives in namespace
/// fl, so an unqualified call on one of them searches fl. If fl declared
/// `template <class T> void swap(T&, T&)` or `void* memcpy(void*, const void*,
/// size_t)` directly, those would join the overload set next to std::swap or
/// ::memcpy and tie. That made legacy code like `memset(leds, 0, n)`,
/// `free(leds)` and even `std::reverse(leds, leds + n)` (libstdc++ calls swap
/// unqualified) fail to compile (FastLED#4827).
///
/// Such functions are declared in fl::adl_barrier instead, and fl imports them
/// with a using-directive. ADL ignores using-directives, so the barrier is
/// invisible to it. Ordinary lookup does follow the directive, so `fl::swap(a,
/// b)`, `fl::memcpy(...)` and unqualified calls inside fl work as before.
///
/// Rules:
/// - Put every namespace-scope overload of a barrier name in the barrier.
///   Qualified `fl::name` lookup stops at fl and skips the directive if fl
///   declares any `name` itself.
///   Lint enforces this (AdlBarrierChecker, FastLED#4851), and also requires
///   each barrier function to be called as `fl::name` in
///   tests/fl/stl/adl_barrier.cpp.
/// - The C library mirrors (fl::memcpy, fl::free, fl::strlen, ...) use a
///   different fix, see fl/stl/cstring.h: they are templates with an unused
///   defaulted parameter, so the non-template ::memcpy wins any tie, including
///   under a using-directive for fl, which a using-directive cannot help with.
/// - swap and clamp are exceptions: it stays directly in fl so `swap(a, b)` on fl
///   types still works via ADL; their generic overloads are written to be
///   less specialized than std::swap / std::clamp so std wins ties (see
///   type_traits.h and fl/math/math.h).
/// - Before 3.10.4 CRGB was a global type, so none of these names were
///   reachable through ADL on a pixel; the barrier restores that behaviour.

namespace fl {
namespace adl_barrier {}
using namespace adl_barrier;  // ok bare using
}  // namespace fl
