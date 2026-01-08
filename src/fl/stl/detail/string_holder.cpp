#include <stdlib.h>
#include <string.h>  // ok include - for strlen

#include "fl/stl/detail/string_holder.h"
#include "fl/stl/cstring.h"  // For memcpy
#include "fl/stl/malloc.h"   // For fl::malloc, fl::free, fl::realloc

namespace fl {

// StringHolder implementations
// Assumes fl::malloc/fl::realloc/fl::free cannot return NULL (OOM is terminal)

StringHolder::StringHolder(const char *str)
    : mData((char*)fl::malloc(strlen(str) + 1))
    , mLength(strlen(str))
    , mCapacity(mLength + 1) {
    fl::memcpy(mData, str, mLength);
    mData[mLength] = '\0';
}

StringHolder::StringHolder(size length)
    : mData((char*)fl::malloc(length + 1))
    , mLength(length)
    , mCapacity(length + 1) {
    mData[mLength] = '\0';
}

StringHolder::StringHolder(const char *str, size length)
    : mData((char*)fl::malloc(length + 1))
    , mLength(length)
    , mCapacity(length + 1) {
    fl::memcpy(mData, str, mLength);
    mData[mLength] = '\0';
}

StringHolder::~StringHolder() {
    fl::free(mData); // Release the memory
}

void StringHolder::grow(size newLength) {
    if (newLength + 1 <= mCapacity) {
        // We have enough capacity for newLength + null terminator
        mLength = newLength;
        mData[mLength] = '\0';
        return;
    }

    // Use fl::realloc for efficient growth without memory move
    // fl::realloc may expand in place or copy to larger block as needed
    mData = (char*)fl::realloc(mData, newLength + 1);
    mLength = newLength;
    mCapacity = newLength + 1;
    mData[mLength] = '\0'; // Ensure null-termination
}

} // namespace fl
