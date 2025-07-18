#pragma once

#include "fl/ptr.h"
#include "fl/shared_ptr.h"
#include "fl/weak_ptr.h"
#include "fl/unique_ptr.h"  // Add this line
#include "fl/type_traits.h"

namespace fl {

// FastLED equivalent of std::intrusive_ptr<T>
// Alias for fl::Ptr<T> to provide familiar intrusive pointer naming
template <typename T>
using intrusive_ptr = fl::Ptr<T>;

// FastLED equivalent of std::make_intrusive<T>
// Creates a new object of type T and returns it wrapped in a fl::Ptr<T>
// 
// Usage:
//   auto ptr = fl::make_intrusive<MyClass>(arg1, arg2, ...);
//
// This is equivalent to:
//   fl::Ptr<MyClass> ptr = fl::make_intrusive<MyClass>(arg1, arg2, ...);
//
// Requirements:
//   - T must inherit from fl::Referent
//   - T must have a constructor that accepts the provided arguments
template <typename T, typename... Args>
fl::Ptr<T> make_intrusive(Args&&... args) {
    return fl::NewPtr<T>(fl::forward<Args>(args)...);
}

// Add make_unique function for consistency with std::make_unique
template<typename T, typename... Args>
typename fl::enable_if<!fl::is_array<T>::value, unique_ptr<T>>::type
make_unique(Args&&... args) {
    return unique_ptr<T>(new T(fl::forward<Args>(args)...));
}

// Add make_unique for arrays
template<typename T>
typename fl::enable_if<fl::is_array<T>::value, unique_ptr<T>>::type
make_unique(fl::size_t size) {
    typedef typename fl::remove_extent<T>::type U;
    return unique_ptr<T>(new U[size]());
}

}
