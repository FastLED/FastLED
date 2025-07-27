// allow-include-after-namespace

#pragma once


#include "fl/namespace.h"

// Define if initializer_list is available
// Check for C++11 and if std::initializer_list exists
#if defined(__AVR__)
// Emulated initializer_list for AVR platforms
namespace fl {
    template<typename T>
    class initializer_list {
    private:
        const T* mBegin;
        fl::size mSize;
        
        // Private constructor used by compiler
        constexpr initializer_list(const T* first, fl::size size)
            : mBegin(first), mSize(size) {}
            
    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using size_type = fl::size;
        using iterator = const T*;
        using const_iterator = const T*;
        
        // Default constructor
        constexpr initializer_list() : mBegin(nullptr), mSize(0) {}
        
        // Size and capacity
        constexpr fl::size size() const { return mSize; }
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
#include <initializer_list>
namespace fl {
    using std::initializer_list;
}
#endif 
