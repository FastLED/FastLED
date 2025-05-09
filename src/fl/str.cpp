#include <stdlib.h>

#include "fl/str.h"

#include "crgb.h"
#include "fl/fft.h"
#include "fl/namespace.h"
#include "fl/unused.h"
#include "fl/xymap.h"

#ifdef FASTLED_TESTING
#include <cstdio> // ok include
#endif

namespace fl {

namespace string_functions {

static void ftoa(float value, char *buffer, int precision = 2) {

    FASTLED_UNUSED(precision);

#ifdef FASTLED_TESTING
    // use sprintf during testing
    sprintf(buffer, "%f", value);
    return;

#else
    // Handle negative values
    if (value < 0) {
        *buffer++ = '-';
        value = -value;
    }

    // Extract integer part
    uint32_t intPart = (uint32_t)value;

    // Convert integer part to string (reversed)
    char intBuf[12]; // Enough for 32-bit integers
    int i = 0;
    do {
        intBuf[i++] = '0' + (intPart % 10);
        intPart /= 10;
    } while (intPart);

    // Write integer part in correct order
    while (i--) {
        *buffer++ = intBuf[i];
    }

    *buffer++ = '.'; // Decimal point

    // Extract fractional part
    float fracPart = value - (uint32_t)value;
    for (int j = 0; j < precision; ++j) {
        fracPart *= 10.0f;
        int digit = (int)fracPart;
        *buffer++ = '0' + digit;
        fracPart -= digit;
    }

    *buffer = '\0'; // Null-terminate
#endif
}

static int itoa(int value, char *sp, int radix) {
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
        v = radix ? v / radix : 0;
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

static float atoff(const char *str, size_t len) {
    float result = 0.0f;   // The resulting number
    float sign = 1.0f;     // Positive or negative
    float fraction = 0.0f; // Fractional part
    float divisor = 1.0f;  // Divisor for the fractional part
    int isFractional = 0;  // Whether the current part is fractional

    size_t pos = 0; // Current position in the string

    // Handle empty input
    if (len == 0) {
        return 0.0f;
    }

    // Skip leading whitespace (manual check instead of isspace)
    while (pos < len &&
           (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' ||
            str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v')) {
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

void StringFormatter::append(int32_t val, StrN<64> *dst) {
    char buf[63] = {0};
    string_functions::itoa(val, buf, 10);
    dst->write(buf, strlen(buf));
}
StringHolder::StringHolder(const char *str) {
    mLength = strlen(str);   // Don't include null terminator in length
    mCapacity = mLength + 1; // Capacity includes null terminator
    mData = new char[mCapacity];
    memcpy(mData, str, mLength);
    mData[mLength] = '\0';
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

Str &Str::append(const FFTBins &str) {
    append("\n FFTImpl Bins:\n  ");
    append(str.bins_raw);
    append("\n");
    append(" FFTImpl Bins DB:\n  ");
    append(str.bins_db);
    append("\n");
    return *this;
}

Str &Str::append(const XYMap &map) {
    append("XYMap(");
    append(map.getWidth());
    append(",");
    append(map.getHeight());
    append(")");
    return *this;
}

void Str::swap(Str &other) {
    if (this != &other) {
        fl::swap(mLength, other.mLength);
        char temp[FASTLED_STR_INLINED_SIZE];
        memcpy(temp, mInlineData, FASTLED_STR_INLINED_SIZE);
        memcpy(mInlineData, other.mInlineData, FASTLED_STR_INLINED_SIZE);
        memcpy(other.mInlineData, temp, FASTLED_STR_INLINED_SIZE);
        fl::swap(mHeapData, other.mHeapData);
    }
}

void Str::compileTimeAssertions() {
    static_assert(FASTLED_STR_INLINED_SIZE > 0,
                  "FASTLED_STR_INLINED_SIZE must be greater than 0");
    static_assert(FASTLED_STR_INLINED_SIZE == kStrInlineSize,
                  "If you want to change the FASTLED_STR_INLINED_SIZE, then it "
                  "must be through a build define and not an include define.");
}

Str &Str::append(const CRGB &rgb) {
    append("CRGB(");
    append(rgb.r);
    append(",");
    append(rgb.g);
    append(",");
    append(rgb.b);
    append(")");
    return *this;
}

void StringFormatter::appendFloat(const float &val, StrN<64> *dst) {
    char buf[64] = {0};
    string_functions::ftoa(val, buf);
    dst->write(buf, strlen(buf));
}

} // namespace fl
