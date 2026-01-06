///////////////////////////////////////////////////////////////////////////////
// FastLED Character Conversion Functions Implementation
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/charconv.h"
#include "fl/str.h"

namespace fl {
namespace detail {

fl::string hex(uint64_t value, HexIntWidth width, bool is_negative, bool uppercase) {
    // Note: width parameter kept for API compatibility but not used for padding
    // Standard printf("%x") doesn't zero-pad by default
    (void)width; // Suppress unused parameter warning

    // Handle zero case - single '0' character (no padding)
    if (value == 0) {
        return fl::string("0");
    }

    fl::string result;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    // Convert value to hex string (minimal representation, no padding)
    while (value > 0) {
        char ch = digits[value % 16];
        // Convert char to string since fl::string::append treats char as number
        char temp_ch_str[2] = {ch, '\0'};
        fl::string digit_str(temp_ch_str);
        // Use += since + operator is not defined for fl::string
        fl::string temp = digit_str;
        temp += result;
        result = temp;
        value /= 16;
    }

    // Add negative sign if needed
    if (is_negative) {
        fl::string minus_str("-");
        minus_str += result;
        result = minus_str;
    }

    return result;
}

} // namespace detail
} // namespace fl
