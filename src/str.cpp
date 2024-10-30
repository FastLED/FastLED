#include <stdlib.h>

#include "str.h"
#include "namespace.h"


FASTLED_NAMESPACE_BEGIN

namespace string_functions {
// Yet, another good itoa implementation
// returns: the length of the number string
int itoa(int value, char *sp, int radix) {
    char tmp[16]; // be careful with the length of the buffer
    char *tp = tmp;
    int i;
    unsigned v;

    int sign = (radix == 10 && value < 0);
    if (sign)
        v = -value;
    else
        v = (unsigned)value;

    while (v || tp == tmp) {
        i = v % radix;
        v /= radix;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    if (sign) {
        *sp++ = '-';
        len++;
    }

    while (tp > tmp)
        *sp++ = *--tp;

    return len;
}

} // namespace string_functions

void StringFormatter::append(int val, StrN<64> *dst) {
    char buf[63];
    string_functions::itoa(val, buf, 63);
    dst->append(buf);
}

StringHolder::StringHolder(const char *str) {
    mLength = strlen(str);
    mData = (char *)malloc(mLength + 1);
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
    char *newData = (char *)realloc(mData, newLength + 1);
    if (newData) {
        mData = newData;
        mLength = newLength;
        mCapacity = newLength;
        mData[mLength] = '\0'; // Ensure null-termination
    } else {
        // handle re-allocation failure.
        char *newData = (char *)malloc(newLength + 1);
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
