#pragma once

#include <stddef.h>
#include "fl/namespace.h"

// Initializer list support is always available
// For AVR platforms, we provide an emulated implementation
// For other platforms, we use std::initializer_list
#define FASTLED_HAS_INITIALIZER_LIST 1

#if defined(__AVR__)
// Emulated initializer_list for AVR platforms
namespace fl {
    template<typename T>
    class initializer_list {
    private:
        const T* mBegin;
        size_t mSize;
        
        // Private constructor used by compiler
        constexpr initializer_list(const T* first, size_t size)
            : mBegin(first), mSize(size) {}
            
    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using size_type = size_t;
        using iterator = const T*;
        using const_iterator = const T*;
        
        // Default constructor
        constexpr initializer_list() : mBegin(nullptr), mSize(0) {}
        
        // Size and capacity
        constexpr size_type size() const { return mSize; }
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
#else
// Use standard library initializer_list for non-AVR platforms
#include <initializer_list>
namespace fl {
    using std::initializer_list;
}
#endif 
