#pragma once

/// @file scope_exit.h
/// @brief RAII scope guard — executes a callable on scope exit.
/// @details Modeled after P0052R10 (Library Fundamentals TS v3) and
///          Boost.Scope scope_exit. Factory function make_scope_exit()
///          provided for C++11 compatibility (no CTAD).

#include "fl/stl/type_traits.h"

namespace fl {

/// @brief RAII guard that executes a callable when destroyed.
/// @tparam EF Exit function type (lambda, functor, function pointer).
template <typename EF>
class scope_exit {
public:
    template <typename EFP>
    explicit scope_exit(EFP &&f)
        : mExitFunction(fl::forward<EFP>(f)), mActive(true) {}

    scope_exit(scope_exit &&rhs)
        : mExitFunction(fl::move(rhs.mExitFunction)), mActive(rhs.mActive) {
        rhs.release();
    }

    scope_exit(const scope_exit &) = delete;
    scope_exit &operator=(const scope_exit &) = delete;
    scope_exit &operator=(scope_exit &&) = delete;

    ~scope_exit() {
        if (mActive) {
            mExitFunction();
        }
    }

    void release() noexcept { mActive = false; }

private:
    EF mExitFunction;
    bool mActive;
};

/// Factory for C++11/14 (no CTAD). Usage:
///   auto guard = fl::make_scope_exit([&]{ cleanup(); });
template <typename EF>
scope_exit<decay_t<EF>> make_scope_exit(EF &&f) {
    return scope_exit<decay_t<EF>>(fl::forward<EF>(f));
}

} // namespace fl
