#pragma once

#include "fl/remote/rpc/type_conversion_result.h"  // IWYU pragma: keep
#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/string.h"  // IWYU pragma: keep
#include "fl/stl/cstdlib.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
namespace fl {
namespace detail {

// =============================================================================
// JsonToIntegerVisitor - Visitor for converting JSON to integer types
// =============================================================================
template <typename T>
struct JsonToIntegerVisitor {
    T mValue;
    TypeConversionResult mResult;

    JsonToIntegerVisitor() FL_NOEXCEPT : mValue(T()) {}

    template<typename U>
    void accept(const U& value) {
        (*this)(value);
    }

    // Exact match: i64
    void operator()(const i64& raw) {
        mValue = static_cast<T>(raw);
        // Check for overflow/truncation
        if (static_cast<i64>(mValue) != raw) {
            mResult.addWarning("integer overflow/truncation: " + fl::to_string(raw) +
                              " converted to " + fl::to_string(static_cast<i64>(mValue)));
        }
    }

    // Boolean to integer
    void operator()(const bool& b) {
        mValue = b ? T(1) : T(0);
        mResult.addWarning("bool converted to int " + fl::to_string(static_cast<i64>(mValue)));
    }

    // Float to integer
    void operator()(const float& raw) {
        mValue = static_cast<T>(raw);
        // Keep the precision check in single precision. The natural
        // formulation `(double)raw != (double)mValue` drags libgcc's
        // soft-double helpers (`__aeabi_dcmpeq`, `__ledf2`, `__gedf2`,
        // `__aeabi_d2f`, ...) into the link on no-FPU targets — ~760 B
        // we can't afford on LPC845 (FastLED #3002).
        if (raw != static_cast<float>(mValue)) {
            mResult.addWarning("float " + fl::to_string(raw, 6) +
                              " truncated to int " + fl::to_string(static_cast<i64>(mValue)));
        }
    }

    // String to integer
    void operator()(const fl::string& str) {
        char* end = nullptr;
        long parsed = fl::strtol(str.c_str(), &end, 10);
        if (end != str.c_str() && *end == '\0') {
            mValue = static_cast<T>(parsed);
            mResult.addWarning("string '" + str + "' parsed to int " + fl::to_string(static_cast<i64>(mValue)));
        } else {
            mResult.setError("cannot parse string '" + str + "' as integer");
        }
    }

    // Null
    void operator()(const fl::nullptr_t&) {
        mResult.setError("cannot convert null to integer");
    }

    // Object (json_object)
    void operator()(const json_object&) {
        mResult.setError("cannot convert object to integer");
    }

    // Array (json_array)
    void operator()(const json_array&) {
        mResult.setError("cannot convert array to integer");
    }

    // Specialized arrays
    void operator()(const fl::vector<i16>&) {
        mResult.setError("cannot convert array to integer");
    }

    void operator()(const fl::vector<u8>&) {
        mResult.setError("cannot convert array to integer");
    }

    void operator()(const fl::vector<float>&) {
        mResult.setError("cannot convert array to integer");
    }
};

// =============================================================================
// JsonToBoolVisitor - Visitor for converting JSON to bool
// =============================================================================
struct JsonToBoolVisitor {
    bool mValue;
    TypeConversionResult mResult;

    JsonToBoolVisitor() FL_NOEXCEPT : mValue(false) {}

    template<typename U>
    void accept(const U& value) {
        (*this)(value);
    }

    // Exact match: bool
    void operator()(const bool& b) {
        mValue = b;
    }

    // Integer to bool
    void operator()(const i64& raw) {
        mValue = raw != 0;
        mResult.addWarning("int " + fl::to_string(raw) + " converted to bool " + (mValue ? "true" : "false"));
    }

    // Float to bool
    void operator()(const float& raw) {
        mValue = raw != 0.0f;
        // No double promotion (see JsonToIntVisitor — same #3002 reason).
        mResult.addWarning("float " + fl::to_string(raw, 6) + " converted to bool " + (mValue ? "true" : "false"));
    }

    // String to bool
    void operator()(const fl::string& str) {
        if (str == "true" || str == "1" || str == "yes") {
            mValue = true;
            mResult.addWarning("string '" + str + "' parsed as bool true");
        } else if (str == "false" || str == "0" || str == "no") {
            mValue = false;
            mResult.addWarning("string '" + str + "' parsed as bool false");
        } else {
            mResult.setError("cannot parse string '" + str + "' as bool");
        }
    }

    // Null
    void operator()(const fl::nullptr_t&) {
        mResult.setError("cannot convert null to bool");
    }

    // Object
    void operator()(const json_object&) {
        mResult.setError("cannot convert object to bool");
    }

    // Array
    void operator()(const json_array&) {
        mResult.setError("cannot convert array to bool");
    }

    // Specialized arrays
    void operator()(const fl::vector<i16>&) {
        mResult.setError("cannot convert array to bool");
    }

    void operator()(const fl::vector<u8>&) {
        mResult.setError("cannot convert array to bool");
    }

    void operator()(const fl::vector<float>&) {
        mResult.setError("cannot convert array to bool");
    }
};

// =============================================================================
// JsonToFloatVisitor - Visitor for converting JSON to float/double
// =============================================================================
template <typename T>
struct JsonToFloatVisitor {
    T mValue;
    TypeConversionResult mResult;

    JsonToFloatVisitor() FL_NOEXCEPT : mValue(T()) {}

    template<typename U>
    void accept(const U& value) {
        (*this)(value);
    }

    // Float to float/double
    void operator()(const float& raw) {
        mValue = static_cast<T>(raw);
    }

    // Integer to float
    void operator()(const i64& raw) {
        mValue = static_cast<T>(raw);
        // Large integers may lose precision when converted to float
        if (sizeof(T) < 8 && (raw > 16777216 || raw < -16777216)) {
            mResult.addWarning("large int " + fl::to_string(raw) + " may lose precision as float");
        }
    }

    // Bool to float
    void operator()(const bool& b) {
        mValue = b ? T(1) : T(0);
        // Format through float to avoid pulling in double helpers on no-FPU
        // targets (FastLED #3002). For T = double the round-trip costs one
        // extra `.0f` digit of formatting precision, which is acceptable.
        mResult.addWarning("bool converted to float " + fl::to_string(static_cast<float>(mValue), 6));
    }

    // String to float
    void operator()(const fl::string& str) {
        char* end = nullptr;
        double parsed = fl::strtod(str.c_str(), &end);
        if (end != str.c_str() && *end == '\0') {
            mValue = static_cast<T>(parsed);
            // Warning string uses float precision (see #3002).
            mResult.addWarning("string '" + str + "' parsed to float " + fl::to_string(static_cast<float>(mValue), 6));
        } else {
            mResult.setError("cannot parse string '" + str + "' as float");
        }
    }

    // Null
    void operator()(const fl::nullptr_t&) {
        mResult.setError("cannot convert null to float");
    }

    // Object
    void operator()(const json_object&) {
        mResult.setError("cannot convert object to float");
    }

    // Array
    void operator()(const json_array&) {
        mResult.setError("cannot convert array to float");
    }

    // Specialized arrays
    void operator()(const fl::vector<i16>&) {
        mResult.setError("cannot convert array to float");
    }

    void operator()(const fl::vector<u8>&) {
        mResult.setError("cannot convert array to float");
    }

    void operator()(const fl::vector<float>&) {
        mResult.setError("cannot convert array to float");
    }
};

// =============================================================================
// JsonToStringVisitor - Visitor for converting JSON to string
// =============================================================================
struct JsonToStringVisitor {
    fl::string mValue;
    TypeConversionResult mResult;

    JsonToStringVisitor() FL_NOEXCEPT {}

    template<typename U>
    void accept(const U& value) {
        (*this)(value);
    }

    // Exact match: string
    void operator()(const fl::string& str) {
        mValue = str;
    }

    // Integer to string
    void operator()(const i64& raw) {
        mValue = fl::to_string(raw);
        mResult.addWarning("int " + mValue + " converted to string");
    }

    // Float to string
    void operator()(const float& raw) {
        // Format in float (see JsonToIntVisitor — same #3002 reason).
        mValue = fl::to_string(raw, 6);
        mResult.addWarning("float " + mValue + " converted to string");
    }

    // Bool to string
    void operator()(const bool& b) {
        mValue = b ? "true" : "false";
        mResult.addWarning("bool converted to string '" + mValue + "'");
    }

    // Null to string
    void operator()(const fl::nullptr_t&) {
        mValue = "null";
        mResult.addWarning("null converted to string 'null'");
    }

    // Object
    void operator()(const json_object&) {
        mResult.setError("cannot convert object to string");
    }

    // Array
    void operator()(const json_array&) {
        mResult.setError("cannot convert array to string");
    }

    // Specialized arrays
    void operator()(const fl::vector<i16>&) {
        mResult.setError("cannot convert array to string");
    }

    void operator()(const fl::vector<u8>&) {
        mResult.setError("cannot convert array to string");
    }

    void operator()(const fl::vector<float>&) {
        mResult.setError("cannot convert array to string");
    }
};

} // namespace detail
} // namespace fl
