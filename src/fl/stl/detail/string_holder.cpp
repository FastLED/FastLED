#include <stdlib.h>
#include <string.h>  // ok include - for strlen

#include "fl/stl/detail/string_holder.h"
#include "fl/stl/cstring.h"  // For memcpy

namespace fl {

// StringHolder implementations
StringHolder::StringHolder(const char *str) {
    mLength = strlen(str);   // Don't include null terminator in length
    mCapacity = mLength + 1; // Capacity includes null terminator
    mData = new char[mCapacity];
    fl::memcpy(mData, str, mLength);
    mData[mLength] = '\0';
}

StringHolder::StringHolder(size length) {
    mData = (char *)malloc(length + 1);
    if (mData) {
        mLength = length;
        mCapacity = length + 1;
        mData[mLength] = '\0';
    } else {
        mLength = 0;
        mCapacity = 0;
    }
}

StringHolder::StringHolder(const char *str, size length) {
    mData = (char *)malloc(length + 1);
    if (mData) {
        mLength = length;
        fl::memcpy(mData, str, mLength);
        mData[mLength] = '\0';
    } else {
        mLength = 0;
    }
    mCapacity = mLength + 1;
}

StringHolder::~StringHolder() {
    free(mData); // Release the memory
}

void StringHolder::grow(size newLength) {
    if (newLength + 1 <= mCapacity) {
        // We have enough capacity for newLength + null terminator
        mLength = newLength;
        mData[mLength] = '\0';
        return;
    }
    char *newData = (char *)realloc(mData, newLength + 1);
    if (newData) {
        mData = newData;
        mLength = newLength;
        mCapacity = newLength + 1;
        mData[mLength] = '\0'; // Ensure null-termination
    } else {
        // handle re-allocation failure.
        char *newData = (char *)malloc(newLength + 1);
        if (newData) {
            fl::memcpy(newData, mData, mLength + 1);
            free(mData);
            mData = newData;
            mLength = newLength;
            mCapacity = mLength + 1;
        } else {
            // memory failure.
        }
    }
}

} // namespace fl
