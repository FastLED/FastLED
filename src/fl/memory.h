#pragma once

#include "fl/ptr.h"
#include "fl/shared_ptr.h"
#include "fl/weak_ptr.h"
#include "fl/unique_ptr.h"  // Add this line
#include "fl/type_traits.h"

namespace fl {

// FastLED equivalent of std::intrusive_ptr<T>
// Alias for fl::Ptr<T> to provide familiar intrusive pointer naming
// NOTE: This is deprecated - use fl::shared_ptr<T> instead
template <typename T>
using intrusive_ptr = fl::Ptr<T>;

// FastLED equivalent of std::make_intrusive<T>
// NOW DELEGATES TO make_shared<T> for migration compatibility
// 
// Usage:
//   auto ptr = fl::make_intrusive<MyClass>(arg1, arg2, ...);
//   // This now creates a fl::shared_ptr<MyClass> instead of fl::Ptr<MyClass>
//
// Requirements:
//   - T no longer needs to inherit from fl::Referent
//   - T must have a constructor that accepts the provided arguments
template <typename T, typename... Args>
fl::shared_ptr<T> make_intrusive(Args&&... args) {
    return fl::make_shared<T>(fl::forward<Args>(args)...);
}

// Convenience factory for the new pattern - equivalent to make_shared
template <typename T, typename... Args>
fl::shared_ptr<T> make_shared_ptr(Args&&... args) {
    return fl::make_shared<T>(fl::forward<Args>(args)...);
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
