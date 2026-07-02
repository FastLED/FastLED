// IWYU pragma: begin_keep
#include <stdlib.h>
#include <string.h>  // okay banned header (STL wrapper implementation requires standard header for strlen)

// IWYU pragma: end_keep
#include "fl/stl/detail/string_holder.h"
#include "fl/stl/cstring.h"  // For memcpy
#include "fl/stl/malloc.h"   // For fl::malloc, fl::free, fl::realloc
#include "fl/stl/noexcept.h"

namespace fl {

// StringHolder implementations
//
// OOM hardening (FastLED#3588): on classic ESP32 with WiFi + an HTTP
// server active, free heap drops below 20 KB and fl::malloc genuinely
// returns NULL. The previous "OOM is terminal" assumption turned that
// into UB (writes through null; realloc leak plus null mData with a
// stale nonzero capacity) and crashed unattended bench devices. On
// failure a holder now degrades to a coherent empty state
// (mData=null, mLength=0, mCapacity=0) and grow() keeps the old
// buffer.

StringHolder::StringHolder(const char *str)
    : mData((char*)fl::malloc(strlen(str) + 1))
    , mLength(strlen(str))
    , mCapacity(mLength + 1) {
    if (mData == nullptr) {
        mLength = 0;
        mCapacity = 0;
        return;
    }
    fl::memcpy(mData, str, mLength);
    mData[mLength] = '\0';
}

StringHolder::StringHolder(size length)
    : mData((char*)fl::malloc(length + 1))
    , mLength(length)
    , mCapacity(length + 1) {
    if (mData == nullptr) {
        mLength = 0;
        mCapacity = 0;
        return;
    }
    mData[mLength] = '\0';
}

StringHolder::StringHolder(const char *str, size length)
    : mData((char*)fl::malloc(length + 1))
    , mLength(length)
    , mCapacity(length + 1) {
    if (mData == nullptr) {
        mLength = 0;
        mCapacity = 0;
        return;
    }
    fl::memcpy(mData, str, mLength);
    mData[mLength] = '\0';
}

StringHolder::~StringHolder() FL_NO_EXCEPT {
    fl::free(mData); // Release the memory
}

void StringHolder::grow(size newLength) {
    if (newLength + 1 <= mCapacity) {
        // We have enough capacity for newLength + null terminator
        mLength = newLength;
        mData[mLength] = '\0';
        return;
    }

    // Use fl::realloc for efficient growth without memory move.
    // Keep the old buffer if the allocation fails — the string simply
    // does not grow (callers observe the unchanged length).
    char *grown = (char*)fl::realloc(mData, newLength + 1);
    if (grown == nullptr) {
        return;
    }
    mData = grown;
    mLength = newLength;
    mCapacity = newLength + 1;
    mData[mLength] = '\0'; // Ensure null-termination
}

} // namespace fl
