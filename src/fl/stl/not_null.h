#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED not_null<T> - Non-null pointer wrapper from C++ Core Guidelines GSL
//
// Provides a lightweight wrapper that enforces non-null pointer semantics at
// compile-time and runtime (in debug builds). Based on the C++ Core Guidelines
// Support Library (GSL).
//
// Key Features:
// - Zero-overhead abstraction in release builds (assertions compiled out)
// - Compile-time prevention of nullptr assignment via deleted constructors
// - Runtime assertion of non-null invariant in debug builds
// - Platform-aware assertions (desktop, ESP32, WASM, AVR)
// - Works with raw pointers, smart pointers, and custom pointer types
//
// Platform-Specific Behavior:
// - Desktop/Host: Full assertion support with detailed error messages
// - ESP32: Serial output via fl::println() with assertion message
// - WASM: Browser console warning + debugger breakpoint (emscripten_debugger)
// - AVR: Lightweight assertion (FASTLED_WARN_IF) due to memory constraints
// - All platforms: Assertions only active in debug builds (FL_DEBUG defined)
//
// Usage Example:
//   // Function parameter - expresses non-null contract in type system
//   void setPixels(fl::not_null<CRGB*> leds, int count) {
//       // No null check needed - guaranteed by type
//       for (int i = 0; i < count; i++) {
//           leds[i] = CRGB::Red;  // Safe dereference
//       }
//   }
//
//   // Call site - compiler enforces non-null at construction
//   CRGB pixels[100];
//   setPixels(pixels, 100);           // OK - implicit conversion
//   setPixels(nullptr, 100);          // Compile error - deleted constructor
//
//   // Works with smart pointers
//   auto uptr = fl::make_unique<int>(42);
//   fl::not_null<int*> nnptr(uptr.get());  // OK if uptr is non-null
//
// References:
// - https://cpp-core-guidelines-docs.vercel.app/gsl
// - https://github.com/microsoft/GSL
// - https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/type_traits.h"
#include "fl/stl/move.h"
#include "fl/stl/cstddef.h"

// Forward declarations to break circular dependency with assert.h
// not_null needs assertions, but assert.h might need not_null in the future
namespace fl {
namespace detail {
    // Low-level assertion for not_null - implementation in not_null.cpp
    // Provides platform-specific failure handling without depending on assert.h
    void not_null_assert_failed(const char* message);
} // namespace detail
} // namespace fl



namespace fl {

// Forward declaration
template<typename T>
class not_null;

namespace detail {

// Type trait: is_reference
// Detects if T is a reference type (lvalue or rvalue reference)
template<typename T>
struct is_reference : fl::false_type {};

template<typename T>
struct is_reference<T&> : fl::true_type {};

template<typename T>
struct is_reference<T&&> : fl::true_type {};

// Type trait: is_comparable_to_nullptr
// Detects if T can be compared to nullptr using SFINAE
template<typename T>
struct is_comparable_to_nullptr {
private:
    template<typename U>
    static auto test(int) -> decltype(fl::declval<U>() == nullptr, fl::true_type{});

    template<typename>
    static fl::false_type test(...);

public:
    enum : bool { value = decltype(test<T>(0))::value };
};

// Type trait: is_dereferenceable
// Detects if T supports operator* (dereference operator)
template<typename T>
struct is_dereferenceable {
private:
    template<typename U>
    static auto test(int) -> decltype(*fl::declval<U>(), fl::true_type{});

    template<typename>
    static fl::false_type test(...);

public:
    enum : bool { value = decltype(test<T>(0))::value };
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////
// not_null<T> - Non-null pointer wrapper
//
// Template class that wraps a pointer-like type T and enforces non-null
// guarantees at construction and assignment. Provides zero-overhead abstraction
// in release builds (assertions compiled out).
//
// Assertion Strategy:
// The implementation uses FL_ASSERT macro which provides platform-specific
// assertion behavior:
// - Debug builds (FL_DEBUG defined): Active runtime checks with error messages
// - Release builds: Assertions are compiled out (zero overhead, undefined behavior if violated)
// - Desktop platforms: Full assertion support via FASTLED_WARN_IF
// - ESP32: Serial output with panic behavior (fl::println + potential halt)
// - WASM: Browser debugger breakpoint (emscripten_debugger())
// - AVR: Lightweight warnings (memory constrained)
//
// The fail-fast approach catches null violations at assignment time (not dereference),
// making bugs easier to diagnose. In release builds, the compiler can optimize
// aggressively knowing the pointer is never null.
//
// Usage:
//   int value = 42;
//   fl::not_null<int*> ptr(&value);  // OK - non-null
//   *ptr = 100;                       // OK - dereference guaranteed safe
//
//   fl::not_null<int*> bad(nullptr); // FAIL - assertion in debug, UB in release
//
// Template requirements:
// - T must be comparable to nullptr (T == nullptr is valid)
// - T must not be a reference type (use pointers instead)
// - T should support dereference operator* (for most use cases)
//
// @tparam T Pointer-like type (raw pointer, smart pointer, custom pointer type)
///////////////////////////////////////////////////////////////////////////////
template<typename T>
class not_null {
private:
    T mPtr;  // Underlying pointer (mCamelCase naming per FastLED standards)

    // Static assertions to enforce template constraints
    static_assert(detail::is_comparable_to_nullptr<T>::value,
                  "not_null<T>: T must be comparable to nullptr");

    static_assert(!detail::is_reference<T>::value,
                  "not_null<T>: T must not be a reference type");

public:
    ///////////////////////////////////////////////////////////////////////////
    // Construction
    ///////////////////////////////////////////////////////////////////////////

    // Deleted default constructor - not_null must always be initialized
    not_null() = delete;

    // Primary constructor - accepts non-null pointer
    // Asserts non-null in debug builds, undefined behavior if null in release
    // Note: Not explicit to allow implicit conversion from T (matching GSL behavior)
    // Note: Not constexpr in C++11 due to FL_NOT_NULL_ASSERT limitation
    not_null(T ptr) : mPtr(ptr) {
        if (mPtr == nullptr) {
            detail::not_null_assert_failed("not_null constructed with nullptr");
        }
    }

    // Copy constructor (defaulted)
    constexpr not_null(const not_null& other) = default;

    // Move constructor (defaulted)
    constexpr not_null(not_null&& other) = default;

    // Converting constructor - allows construction from compatible pointer types
    // Example: not_null<Base*> from not_null<Derived*>
    template<typename U>
    not_null(const not_null<U>& other) : mPtr(other.get()) {
        // No assertion needed - other already guarantees non-null
    }

    // Deleted nullptr constructor - prevents construction with nullptr at compile-time
    not_null(fl::nullptr_t) = delete;

    ///////////////////////////////////////////////////////////////////////////
    // Assignment
    ///////////////////////////////////////////////////////////////////////////

    // Copy assignment (defaulted)
    not_null& operator=(const not_null& other) = default;

    // Move assignment (defaulted)
    not_null& operator=(not_null&& other) = default;

    // Assignment from raw pointer - asserts non-null in debug builds
    not_null& operator=(T ptr) {
        if (ptr == nullptr) {
            detail::not_null_assert_failed("not_null assigned nullptr");
        }
        mPtr = ptr;
        return *this;
    }

    // Deleted nullptr assignment - prevents assignment of nullptr at compile-time
    not_null& operator=(fl::nullptr_t) = delete;

    ///////////////////////////////////////////////////////////////////////////
    // Access operators
    ///////////////////////////////////////////////////////////////////////////

    // Get underlying pointer (explicit)
    constexpr T get() const noexcept {
        return mPtr;
    }

    // Implicit conversion to underlying pointer type
    // Allows seamless integration with existing APIs expecting raw pointers
    constexpr operator T() const noexcept {
        return mPtr;
    }

    // Arrow operator - for member access
    constexpr T operator->() const noexcept {
        return mPtr;
    }

    // Dereference operator - returns reference to pointed-to object
    constexpr auto operator*() const -> decltype(*fl::declval<T>()) {
        return *mPtr;
    }

    // Array subscript operator - for pointer-to-array types
    template<typename U = T>
    constexpr auto operator[](fl::size_t index) const
        -> decltype(fl::declval<U>()[index]) {
        return mPtr[index];
    }

    ///////////////////////////////////////////////////////////////////////////
    // Comparison operators
    ///////////////////////////////////////////////////////////////////////////

    // Equality comparison with another not_null
    template<typename U>
    constexpr bool operator==(const not_null<U>& other) const {
        return mPtr == other.get();
    }

    // Inequality comparison with another not_null
    template<typename U>
    constexpr bool operator!=(const not_null<U>& other) const {
        return mPtr != other.get();
    }

    // Equality comparison with raw pointer
    template<typename U>
    constexpr bool operator==(U other) const {
        return mPtr == other;
    }

    // Inequality comparison with raw pointer
    template<typename U>
    constexpr bool operator!=(U other) const {
        return mPtr != other;
    }

    // Less-than comparison (for use in ordered containers)
    template<typename U>
    constexpr bool operator<(const not_null<U>& other) const {
        return mPtr < other.get();
    }

    // Less-than-or-equal comparison
    template<typename U>
    constexpr bool operator<=(const not_null<U>& other) const {
        return mPtr <= other.get();
    }

    // Greater-than comparison
    template<typename U>
    constexpr bool operator>(const not_null<U>& other) const {
        return mPtr > other.get();
    }

    // Greater-than-or-equal comparison
    template<typename U>
    constexpr bool operator>=(const not_null<U>& other) const {
        return mPtr >= other.get();
    }
};

///////////////////////////////////////////////////////////////////////////////
// Non-member comparison operators (for symmetry)
///////////////////////////////////////////////////////////////////////////////

// Allow comparison: raw_ptr == not_null
template<typename T, typename U>
constexpr bool operator==(T lhs, const not_null<U>& rhs) {
    return lhs == rhs.get();
}

// Allow comparison: raw_ptr != not_null
template<typename T, typename U>
constexpr bool operator!=(T lhs, const not_null<U>& rhs) {
    return lhs != rhs.get();
}

} // namespace fl
