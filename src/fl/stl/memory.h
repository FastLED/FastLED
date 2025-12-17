#pragma once

// Convenience header that includes all smart pointer types
// For better compile times, prefer including only what you need:
//   - fl/shared_ptr.h for shared_ptr (includes FASTLED_SHARED_PTR macros)
//   - fl/weak_ptr.h for weak_ptr
//   - fl/unique_ptr.h for unique_ptr and make_unique

#include "fl/stl/shared_ptr.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/unique_ptr.h"

namespace fl {

// Convenience factory - just an alias for fl::make_shared
template <typename T, typename... Args>
fl::shared_ptr<T> make_shared_ptr(Args&&... args) {
    return fl::make_shared<T>(fl::forward<Args>(args)...);
}

}
