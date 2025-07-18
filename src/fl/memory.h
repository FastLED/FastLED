#pragma once

#include "fl/ptr.h"
#include "fl/type_traits.h"

namespace fl {

// FastLED equivalent of std::shared_ptr<T>
// Alias for fl::Ptr<T> to provide familiar std:: naming
template <typename T>
using shared_ptr = fl::Ptr<T>;

// FastLED equivalent of std::make_shared<T>
// Creates a new object of type T and returns it wrapped in a fl::Ptr<T>
// 
// Usage:
//   auto ptr = fl::make_shared<MyClass>(arg1, arg2, ...);
//
// This is equivalent to:
//   fl::Ptr<MyClass> ptr = fl::make_shared<MyClass>(arg1, arg2, ...);
//
// Requirements:
//   - T must inherit from fl::Referent
//   - T must have a constructor that accepts the provided arguments
template <typename T, typename... Args>
fl::Ptr<T> make_shared(Args&&... args) {
    return fl::NewPtr<T>(fl::forward<Args>(args)...);
}

}
