

#pragma once


// Define if initializer_list is available
// Check for C++11 and if std::initializer_list exists
#if !defined(__AVR__)
#include <initializer_list>
#endif

#if defined(__AVR__)
// Emulated initializer_list for AVR platforms
// MUST be in std namespace for compiler's brace-initialization magic to work

namespace std {
    template<typename T>
    class initializer_list {
    private:
        const T* mBegin;
        unsigned int mSize;  // Use unsigned int directly for AVR (16-bit)

        // Private constructor used by compiler
        constexpr initializer_list(const T* first, unsigned int size)
            : mBegin(first), mSize(size) {}

    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using size_type = unsigned int;  // Use unsigned int directly for AVR (16-bit)
        using iterator = const T*;
        using const_iterator = const T*;

        // Default constructor
        constexpr initializer_list() : mBegin(nullptr), mSize(0) {}

        // Size and capacity
        constexpr unsigned int size() const { return mSize; }
        constexpr bool empty() const { return mSize == 0; }

        // Iterators
        constexpr const_iterator begin() const { return mBegin; }
        constexpr const_iterator end() const { return mBegin + mSize; }

        // Allow compiler access to private constructor
        template<typename U> friend class initializer_list;
    };

    // Helper functions to match std::initializer_list interface
    template<typename T>
    constexpr const T* begin(initializer_list<T> il) {
        return il.begin();
    }

    template<typename T>
    constexpr const T* end(initializer_list<T> il) {
        return il.end();
    }
}

// Alias in fl namespace for consistency
namespace fl {
    using std::initializer_list;  // okay std namespace
}
#else
namespace fl {
    using std::initializer_list;  // okay std namespace
}
#endif 
