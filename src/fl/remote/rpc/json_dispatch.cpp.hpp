// IWYU pragma: private, include "fl/remote/rpc/json_dispatch.h"
//
// json_dispatch: implementation of the non-template JSON-to-primitive
// conversion cores. See `json_dispatch.h` for design rationale
// (FastLED #3247 / #3244 Tier 2E Phase 2).

#include "fl/remote/rpc/json_dispatch.h"
#include "fl/stl/json/types.h"
#include "fl/stl/string.h"
#include "fl/stl/cstdlib.h"  // fl::strtol
#include "fl/system/sketch_macros.h"  // FL_PLATFORM_HAS_LARGE_MEMORY

namespace fl {
namespace detail {

FL_NO_INLINE
TypeConversionResult json_convert_to_i64_core(const json_value& val,
                                              fl::i64* out_value) FL_NOEXCEPT {
    TypeConversionResult result;
    if (auto* p = val.data.template ptr<i64>()) {
        *out_value = *p;
        return result;
    }
    if (auto* p = val.data.template ptr<bool>()) {
        *out_value = *p ? 1 : 0;
        result.addWarning(fl::string("bool converted to int ") + fl::to_string(*out_value));
        return result;
    }
    if (auto* p = val.data.template ptr<float>()) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
        *out_value = static_cast<i64>(*p);
        if (*p != static_cast<float>(*out_value)) {
            result.addWarning(fl::string("float ") + fl::to_string(*p, 6) +
                              " truncated to int " + fl::to_string(*out_value));
        }
#else
        (void)p;
        *out_value = 0;
        result.setError("float -> int not supported on Low-memory");
#endif
        return result;
    }
    if (auto* p = val.data.template ptr<fl::string>()) {
        char* end = nullptr;
        long parsed = fl::strtol(p->c_str(), &end, 10);
        if (end != p->c_str() && *end == '\0') {
            *out_value = static_cast<i64>(parsed);
            result.addWarning(fl::string("string '") + *p + "' parsed to int " + fl::to_string(*out_value));
        } else {
            result.setError(fl::string("cannot parse string '") + *p + "' as integer");
        }
        return result;
    }
    if (val.data.template ptr<fl::nullptr_t>()) {
        result.setError("cannot convert null to integer");
        return result;
    }
    // Any remaining alternative (json_object, json_array, vector<i16>,
    // vector<u8>, vector<float>) -- all are "container" types that don't
    // convert cleanly to an integer.
    result.setError("cannot convert array or object to integer");
    *out_value = 0;
    return result;
}

FL_NO_INLINE
TypeConversionResult json_convert_to_bool_core(const json_value& val,
                                               bool* out_value) FL_NOEXCEPT {
    TypeConversionResult result;
    if (auto* p = val.data.template ptr<bool>()) {
        *out_value = *p;
        return result;
    }
    if (auto* p = val.data.template ptr<i64>()) {
        *out_value = (*p != 0);
        result.addWarning(fl::string("int ") + fl::to_string(*p) +
                          " converted to bool " + (*out_value ? "true" : "false"));
        return result;
    }
    if (auto* p = val.data.template ptr<float>()) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
        *out_value = (*p != 0.0f);
        result.addWarning(fl::string("float ") + fl::to_string(*p, 6) +
                          " converted to bool " + (*out_value ? "true" : "false"));
#else
        (void)p;
        *out_value = false;
        result.setError("float -> bool not supported on Low-memory");
#endif
        return result;
    }
    if (auto* p = val.data.template ptr<fl::string>()) {
        if (*p == "true" || *p == "1" || *p == "yes") {
            *out_value = true;
            result.addWarning(fl::string("string '") + *p + "' parsed as bool true");
        } else if (*p == "false" || *p == "0" || *p == "no") {
            *out_value = false;
            result.addWarning(fl::string("string '") + *p + "' parsed as bool false");
        } else {
            result.setError(fl::string("cannot parse string '") + *p + "' as bool");
            *out_value = false;
        }
        return result;
    }
    if (val.data.template ptr<fl::nullptr_t>()) {
        result.setError("cannot convert null to bool");
        *out_value = false;
        return result;
    }
    result.setError("cannot convert array or object to bool");
    *out_value = false;
    return result;
}

FL_NO_INLINE
TypeConversionResult json_convert_to_float_core(const json_value& val,
                                                float* out_value) FL_NOEXCEPT {
    TypeConversionResult result;
    if (auto* p = val.data.template ptr<float>()) {
        *out_value = *p;
        return result;
    }
    if (auto* p = val.data.template ptr<i64>()) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
        *out_value = static_cast<float>(*p);
        if (sizeof(float) < 8 && (*p > 16777216 || *p < -16777216)) {
            result.addWarning(fl::string("large int ") + fl::to_string(*p) +
                              " may lose precision as float");
        }
#else
        // Avoid __aeabi_l2f via i32 route (same trick as #3076).
        if (*p > 2147483647LL) {
            *out_value = static_cast<float>(2147483647);
        } else if (*p < -2147483648LL) {
            *out_value = static_cast<float>(-2147483648);
        } else {
            *out_value = static_cast<float>(static_cast<fl::i32>(*p));
        }
#endif
        return result;
    }
    if (auto* p = val.data.template ptr<bool>()) {
        *out_value = *p ? 1.0f : 0.0f;
        result.addWarning(fl::string("bool converted to float ") +
                          (*p ? "1.0" : "0.0"));
        return result;
    }
    if (auto* p = val.data.template ptr<fl::string>()) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
        char* end = nullptr;
        double parsed = fl::strtod(p->c_str(), &end);
        if (end != p->c_str() && *end == '\0') {
            *out_value = static_cast<float>(parsed);
            result.addWarning(fl::string("string '") + *p + "' parsed to float");
        } else {
            result.setError(fl::string("cannot parse string '") + *p + "' as float");
            *out_value = 0.0f;
        }
#else
        (void)p;
        *out_value = 0.0f;
        result.setError("string -> float not supported on Low-memory");
#endif
        return result;
    }
    if (val.data.template ptr<fl::nullptr_t>()) {
        result.setError("cannot convert null to float");
        *out_value = 0.0f;
        return result;
    }
    result.setError("cannot convert array or object to float");
    *out_value = 0.0f;
    return result;
}

FL_NO_INLINE
TypeConversionResult json_convert_to_string_core(const json_value& val,
                                                 fl::string* out_value) FL_NOEXCEPT {
    TypeConversionResult result;
    if (auto* p = val.data.template ptr<fl::string>()) {
        *out_value = *p;
        return result;
    }
    if (auto* p = val.data.template ptr<i64>()) {
        *out_value = fl::to_string(*p);
        result.addWarning(fl::string("int ") + *out_value + " converted to string");
        return result;
    }
    if (auto* p = val.data.template ptr<float>()) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
        *out_value = fl::to_string(*p, 6);
        result.addWarning(fl::string("float ") + *out_value + " converted to string");
#else
        (void)p;
        *out_value = "0";
        result.setError("float -> string not supported on Low-memory");
#endif
        return result;
    }
    if (auto* p = val.data.template ptr<bool>()) {
        *out_value = *p ? "true" : "false";
        result.addWarning(fl::string("bool converted to string '") + *out_value + "'");
        return result;
    }
    if (val.data.template ptr<fl::nullptr_t>()) {
        *out_value = "null";
        result.addWarning("null converted to string 'null'");
        return result;
    }
    result.setError("cannot convert array or object to string");
    out_value->clear();
    return result;
}

} // namespace detail
} // namespace fl
