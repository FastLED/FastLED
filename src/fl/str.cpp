#include <stdlib.h>

#include "fl/str.h"
#include "namespace.h"


namespace fl {

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

float atoff(const char *str, size_t len) {
    float result = 0.0f;    // The resulting number
    float sign = 1.0f;      // Positive or negative
    float fraction = 0.0f;  // Fractional part
    float divisor = 1.0f;   // Divisor for the fractional part
    int isFractional = 0;   // Whether the current part is fractional

    size_t pos = 0; // Current position in the string

    // Handle empty input
    if (len == 0) {
        return 0.0f;
    }

    // Skip leading whitespace (manual check instead of isspace)
    while (pos < len && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v')) {
        pos++;
    }

    // Handle optional sign
    if (pos < len && str[pos] == '-') {
        sign = -1.0f;
        pos++;
    } else if (pos < len && str[pos] == '+') {
        pos++;
    }

    // Main parsing loop
    while (pos < len) {
        if (str[pos] >= '0' && str[pos] <= '9') {
            if (isFractional) {
                divisor *= 10.0f;
                fraction += (str[pos] - '0') / divisor;
            } else {
                result = result * 10.0f + (str[pos] - '0');
            }
        } else if (str[pos] == '.' && !isFractional) {
            isFractional = 1;
        } else {
            // Stop parsing at invalid characters
            break;
        }
        pos++;
    }

    // Combine integer and fractional parts
    result = result + fraction;

    // Apply the sign
    return sign * result;
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


StringHolder::StringHolder(const char *str, size_t length) {
    mData = (char *)malloc(length + 1);
    if (mData) {
        mLength = length;
        memcpy(mData, str, mLength);
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

float StringFormatter::parseFloat(const char *str, size_t len) {
    return string_functions::atoff(str, len);
}


}