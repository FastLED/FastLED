#pragma once

#include "fl/stl/json.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/remote/rpc/base64.h"
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

// Explicit unsigned-int specialization. Without this, `json(u32_value)` in
// the primary template above is ambiguous between `json(int)` and
// `json(i64)` for a `u32` argument — GCC 15 reports:
//     error: call of overloaded 'json(const long unsigned int&)' is ambiguous
// Route u32 → i64 explicitly so the RPC framework can carry unsigned 32-bit
// return values / arg types (e.g. `fl::u32 rgb` in the LPC PWM+DMA
// AutoResearch RPCs from #3517).
template <>
struct TypeToJson<fl::u32> {
    static json convert(const fl::u32& value) FL_NO_EXCEPT {
        return json(static_cast<fl::i64>(value));
    }
};

// Sibling specialization for u16 which would hit the same ambiguity on
// GCC 15 despite implicit promotion — safer to be explicit.
template <>
struct TypeToJson<fl::u16> {
    static json convert(const fl::u16& value) FL_NO_EXCEPT {
        return json(static_cast<fl::i64>(value));
    }
};

// json identity conversion - pass json through unchanged
template <>
struct TypeToJson<fl::json> {
    static json convert(const json& value) {
        return value;
    }
};

// fl::vector<fl::u8> specialization - encodes binary data as base64 string.
template <>
struct TypeToJson<fl::vector<fl::u8>> {
    static json convert(const fl::vector<fl::u8>& value) {
        fl::string encoded = fl::base64_encode(value);
        return json(encoded.c_str());
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
