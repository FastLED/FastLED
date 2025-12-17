///////////////////////////////////////////////////////////////////////////////
// FastLED Character Conversion Functions Implementation
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/charconv.h"
#include "fl/str.h"

namespace fl {
namespace detail {

fl::string hex(uint64_t value, HexIntWidth width, bool is_negative, bool uppercase) {
    // Calculate expected number of hex characters based on bit width
    size_t expected_chars = static_cast<uint8_t>(width) / 4;

    // Handle zero case - pad to expected width
    if (value == 0) {
        fl::string result;
        for (size_t i = 0; i < expected_chars; ++i) {
            result += "0";
        }
        return result;
    }

    fl::string result;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    // Convert value to hex string
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

    // Pad with leading zeros to reach expected width
    while (result.size() < expected_chars) {
        fl::string temp = "0";
        temp += result;
        result = temp;
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
