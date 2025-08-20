#include <stdlib.h>
#include <string.h>  // ok include - for strcmp
#include "fl/str.h"

#include "crgb.h"
#include "fl/fft.h"
#include "fl/namespace.h"
#include "fl/unused.h"
#include "fl/xymap.h"
#include "fl/json.h"
#include "fl/tile2x2.h"
#include "fl/compiler_control.h"
// UI dependency moved to separate compilation unit to break dependency chain

#ifdef FASTLED_TESTING
#include <cstdio> // ok include
#endif

namespace fl {

// Define static const member for npos (only for string class)
const fl::size string::npos;

// Explicit template instantiations for commonly used sizes
template class StrN<64>;

namespace string_functions {

static void ftoa(float value, char *buffer, int precision = 2) {

#ifdef FASTLED_TESTING
    // use snprintf during testing with precision
    snprintf(buffer, 64, "%.*f", precision, value);
    return;

#else
    // Handle negative values
    if (value < 0) {
        *buffer++ = '-';
        value = -value;
    }

    // Extract integer part
    u32 intPart = (u32)value;

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
    float fracPart = value - (u32)value;
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

static int utoa32(uint32_t value, char *sp, int radix) {
    char tmp[16]; // be careful with the length of the buffer
    char *tp = tmp;
    int i;
    uint32_t v = value;

    while (v || tp == tmp) {
        i = v % radix;
        v = radix ? v / radix : 0;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    while (tp > tmp)
        *sp++ = *--tp;

    return len;
}

static int utoa64(uint64_t value, char *sp, int radix) {
    char tmp[32]; // larger buffer for 64-bit values
    char *tp = tmp;
    int i;
    uint64_t v = value;

    while (v || tp == tmp) {
        i = v % radix;
        v = radix ? v / radix : 0;
        if (i < 10)
            *tp++ = i + '0';
        else
            *tp++ = i + 'a' - 10;
    }

    int len = tp - tmp;

    while (tp > tmp)
        *sp++ = *--tp;

    return len;
}

static float atoff(const char *str, fl::size len) {
    float result = 0.0f;   // The resulting number
    float sign = 1.0f;     // Positive or negative
    float fraction = 0.0f; // Fractional part
    float divisor = 1.0f;  // Divisor for the fractional part
    int isFractional = 0;  // Whether the current part is fractional

    fl::size pos = 0; // Current position in the string

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

void StringFormatter::append(i32 val, StrN<64> *dst) {
    char buf[63] = {0};
    string_functions::itoa(val, buf, 10);
    dst->write(buf, strlen(buf));
}

void StringFormatter::append(u32 val, StrN<64> *dst) {
    char buf[63] = {0};
    string_functions::utoa32(val, buf, 10);
    dst->write(buf, strlen(buf));
}

void StringFormatter::append(uint64_t val, StrN<64> *dst) {
    char buf[63] = {0};
    string_functions::utoa64(val, buf, 10);
    dst->write(buf, strlen(buf));
}

void StringFormatter::append(i16 val, StrN<64> *dst) {
    append(static_cast<i32>(val), dst);
}

void StringFormatter::append(u16 val, StrN<64> *dst) {
    append(static_cast<u32>(val), dst);
}
StringHolder::StringHolder(const char *str) {
    mLength = strlen(str);   // Don't include null terminator in length
    mCapacity = mLength + 1; // Capacity includes null terminator
    mData = new char[mCapacity];
    memcpy(mData, str, mLength);
    mData[mLength] = '\0';
}
StringHolder::StringHolder(fl::size length) {
    mData = (char *)malloc(length + 1);
    if (mData) {
        mLength = length;
        mData[mLength] = '\0';
    } else {
        mLength = 0;
    }
    mCapacity = mLength;
}

StringHolder::StringHolder(const char *str, fl::size length) {
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

void StringHolder::grow(fl::size newLength) {
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

float StringFormatter::parseFloat(const char *str, fl::size len) {
    return string_functions::atoff(str, len);
}

int StringFormatter::parseInt(const char *str, fl::size len) {
    float f = parseFloat(str, len);
    return static_cast<int>(f);
}

int StringFormatter::parseInt(const char *str) {
    return parseInt(str, strlen(str));
}

int string::strcmp(const string& a, const string& b) {
    return ::strcmp(a.c_str(), b.c_str());
}

string &string::append(const FFTBins &str) {
    append("\n FFTImpl Bins:\n  ");
    append(str.bins_raw);
    append("\n");
    append(" FFTImpl Bins DB:\n  ");
    append(str.bins_db);
    append("\n");
    return *this;
}

string &string::append(const XYMap &map) {
    append("XYMap(");
    append(map.getWidth());
    append(",");
    append(map.getHeight());
    append(")");
    return *this;
}

string &string::append(const Tile2x2_u8_wrap &tile) {
    Tile2x2_u8_wrap::Entry data[4] = {
        tile.at(0, 0),
        tile.at(0, 1),
        tile.at(1, 0),
        tile.at(1, 1),
    };

    append("Tile2x2_u8_wrap(");
    for (int i = 0; i < 4; i++) {
        vec2<u16> pos = data[i].first;
        u8 alpha = data[i].second;
        append("(");
        append(pos.x);
        append(",");
        append(pos.y);
        append(",");
        append(alpha);
        append(")");
        if (i < 3) {
            append(",");
        }
    }
    append(")");
    return *this;
}

void string::swap(string &other) {
    if (this != &other) {
        fl::swap(mLength, other.mLength);
        char temp[FASTLED_STR_INLINED_SIZE];
        memcpy(temp, mInlineData, FASTLED_STR_INLINED_SIZE);
        memcpy(mInlineData, other.mInlineData, FASTLED_STR_INLINED_SIZE);
        memcpy(other.mInlineData, temp, FASTLED_STR_INLINED_SIZE);
        fl::swap(mHeapData, other.mHeapData);
    }
}

void string::compileTimeAssertions() {
    static_assert(FASTLED_STR_INLINED_SIZE > 0,
                  "FASTLED_STR_INLINED_SIZE must be greater than 0");
    static_assert(FASTLED_STR_INLINED_SIZE == kStrInlineSize,
                  "If you want to change the FASTLED_STR_INLINED_SIZE, then it "
                  "must be through a build define and not an include define.");
}

string &string::append(const CRGB &rgb) {
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

void StringFormatter::appendFloat(const float &val, StrN<64> *dst, int precision) {
    char buf[64] = {0};
    string_functions::ftoa(val, buf, precision);
    dst->write(buf, strlen(buf));
}

// JsonUiInternal append implementation moved to str_ui.cpp to break dependency chain

// JSON type append implementations
// NOTE: These use forward declarations to avoid circular dependency with json.h
string &string::append(const JsonValue& val) {
    // Use the JsonValue's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    FL_UNUSED(val);
    append("<JsonValue>");
    return *this;
}

string &string::append(const Json& val) {
    // Use the Json's to_string method if available  
    // For now, just append a placeholder to avoid compilation errors
    //append("<Json>");
    append("Json(");
    append(val.to_string());
    append(")");
    return *this;
}

} // namespace fl
