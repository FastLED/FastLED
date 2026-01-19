#pragma once

#include "fl/json.h"
#include "fl/stl/string.h"

#if FASTLED_ENABLE_JSON

namespace fl {
namespace detail {

// =============================================================================
// Return type to JSON converter
// =============================================================================

template <typename T>
struct TypeToJson {
    static Json convert(const T& value) {
        return Json(value);
    }
};

template <>
struct TypeToJson<fl::string> {
    static Json convert(const fl::string& value) {
        return Json(value.c_str());
    }
};

template <>
struct TypeToJson<void> {
    static Json convert() {
        return Json(nullptr);
    }
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
