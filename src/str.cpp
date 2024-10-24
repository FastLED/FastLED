#include "str.h"
#include <stdlib.h>


StringHolder::StringHolder(const char *str) {
    mLength = strlen(str);
    mData = (char*)malloc(mLength + 1);
    if (mData) {
        memcpy(mData, str, mLength + 1);
    } else {
        mLength = 0;
        // Handle allocation failure if necessary
    }
}

StringHolder::~StringHolder() {
    free(mData); // Release the memory
}


void StringHolder::grow(size_t newLength) {
    if (newLength <= mLength) {
        // New length must be greater than current length
        return;
    }
    char* newData = (char*)realloc(mData, newLength + 1);
    if (newData) {
        mData = newData;
        mLength = newLength;
        mData[mLength] = '\0'; // Ensure null-termination
    } else {
        // handle re-allocation failure.
        char* newData = (char*)malloc(newLength + 1);
        if (newData) {
            memcpy(newData, mData, mLength + 1);
            free(mData);
            mData = newData;
            mLength = newLength;
        }
    }
}


