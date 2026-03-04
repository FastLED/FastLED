#pragma once

#include "fl/stl/json.h"
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
    static json convert(const T& value) {
        return json(value);
    }
};

template <>
struct TypeToJson<fl::string> {
    static json convert(const fl::string& value) {
        return json(value.c_str());
    }
};

// json identity conversion - pass json through unchanged
template <>
struct TypeToJson<fl::json> {
    static json convert(const json& value) {
        return value;
    }
};

// fl::vector<T> conversion - converts vector to JSON array
template <typename T>
struct TypeToJson<fl::vector<T>> {
    static json convert(const fl::vector<T>& value) {
        json arr = json::array();
        for (fl::size i = 0; i < value.size(); i++) {
            arr.push_back(TypeToJson<T>::convert(value[i]));
        }
        return arr;
    }
};

template <>
struct TypeToJson<void> {
    static json convert() {
        return json(nullptr);
    }
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
