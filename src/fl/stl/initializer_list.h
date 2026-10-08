
#pragma once

// IWYU pragma: no_include "__cstddef/size_t.h"
// IWYU pragma: no_include "__config"
// IWYU pragma: no_include "version"

#include "fl/stl/noexcept.h"
#include "platforms/is_platform.h"  // IWYU pragma: keep

// Define if initializer_list is available
// Check for C++11 and if std::initializer_list exists
#if !defined(FL_IS_AVR)
// IWYU pragma: begin_keep
#include <initializer_list>  // IWYU pragma: keep
// IWYU pragma: end_keep
#endif

#if defined(FL_IS_AVR)
// Emulated initializer_list for AVR platforms
// MUST be in std namespace for compiler's brace-initialization magic to work

namespace std {
    template<typename T>
    class initializer_list {
    private:
        const T* mBegin;
        unsigned int mSize;  // Use unsigned int directly for AVR (16-bit)

        // Private constructor used by compiler
        constexpr initializer_list(const T* first, unsigned int size) FL_NO_EXCEPT
            : mBegin(first), mSize(size) {}

    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using size_type = unsigned int;  // Use unsigned int directly for AVR (16-bit)
        using iterator = const T*;
        using const_iterator = const T*;

        // Default constructor
        constexpr initializer_list() FL_NO_EXCEPT : mBegin(nullptr), mSize(0) {}

        // Size and capacity
        constexpr unsigned int size() const FL_NO_EXCEPT { return mSize; }
        constexpr bool empty() const FL_NO_EXCEPT { return mSize == 0; }

        // Iterators
        constexpr const_iterator begin() const FL_NO_EXCEPT { return mBegin; }
        constexpr const_iterator end() const FL_NO_EXCEPT { return mBegin + mSize; }

        // Allow compiler access to private constructor
        template<typename U> friend class initializer_list;
    };  // IWYU pragma: keep

    // Helper functions to match std::initializer_list interface  // IWYU pragma: keep
    template<typename T>
    constexpr const T* begin(initializer_list<T> il) FL_NO_EXCEPT {
        return il.begin();
    }

    template<typename T>
    constexpr const T* end(initializer_list<T> il) FL_NO_EXCEPT {
        return il.end();
    }
}

// Alias in fl namespace for consistency
namespace fl {
    using std::initializer_list;  // okay std namespace // ok bare using
}
#else
namespace fl {
    using std::initializer_list;  // okay std namespace // ok bare using
}
#endif 
