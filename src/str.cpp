#include "str.h"
#include <stdlib.h>
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


StringHolder::StringHolder(const char *str) {
    mLength = strlen(str);
    mData = (char*)malloc(mLength + 1);
    if (mData) {
        memcpy(mData, str, mLength + 1);
    } else {
        mLength = 0;
    }
    mCapacity = mLength;
}

StringHolder::StringHolder(size_t length) {
    mData = (char *)malloc(length + 1);
    if (mData) {
        mLength = length;
        mData[mLength] = '\0';
    } else {
        mLength = 0;
    }
    mCapacity = mLength;
}

StringHolder::~StringHolder() {
    free(mData); // Release the memory
}


void StringHolder::grow(size_t newLength) {
    if (newLength <= mCapacity) {
        // New length must be greater than current length
        mLength = newLength;
        return;
    }
    char* newData = (char*)realloc(mData, newLength + 1);
    if (newData) {
        mData = newData;
        mLength = newLength;
        mCapacity = newLength;
        mData[mLength] = '\0'; // Ensure null-termination
    } else {
        // handle re-allocation failure.
        char* newData = (char*)malloc(newLength + 1);
        if (newData) {
            memcpy(newData, mData, mLength + 1);
            free(mData);
            mData = newData;
            mLength = newLength;
            mCapacity = mLength;
        } else {
            // memory failure.
        }
    }
}

FASTLED_NAMESPACE_END
