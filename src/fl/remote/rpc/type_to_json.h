#pragma once

#include "fl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

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

// Json identity conversion - pass Json through unchanged
template <>
struct TypeToJson<fl::Json> {
    static Json convert(const Json& value) {
        return value;
    }
};

// fl::vector<T> conversion - converts vector to JSON array
template <typename T>
struct TypeToJson<fl::vector<T>> {
    static Json convert(const fl::vector<T>& value) {
        Json arr = Json::array();
        for (fl::size i = 0; i < value.size(); i++) {
            arr.push_back(TypeToJson<T>::convert(value[i]));
        }
        return arr;
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
