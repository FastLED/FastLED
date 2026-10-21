// src/fl/remote/transport/serial.cpp.hpp
// Implementation of JSON-RPC serial transport layer

#pragma once

#include "fl/remote/transport/serial.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

namespace fl {

fl::string formatJsonResponse(const fl::json& response, const char* prefix) FL_NO_EXCEPT {
    // Reuse the serialized body. A second growing stream kept two full copies
    // of large discovery responses alive and could exhaust ESP32's heap.
    fl::string jsonStr = response.to_string();
    const fl::size bodyLength = jsonStr.size();
    const fl::size prefixLength = prefix ? fl::strlen(prefix) : 0;
    if (prefixLength != 0) {
        const fl::size totalLength = bodyLength + prefixLength;
        if (totalLength >= bodyLength && totalLength != fl::string::npos) {
            jsonStr.reserve(totalLength);
            jsonStr.write(prefix, prefixLength);
        }
        if (jsonStr.size() != totalLength || totalLength < bodyLength) {
            // Release the large body before allocating the small error reply.
            // A failed extension must not publish partially formed JSON.
            jsonStr.clear(true);
            static const char error[] =
                "{\"error\":{\"code\":-32603,\"message\":\"Out of memory\"},"
                "\"id\":null,\"jsonrpc\":\"2.0\"}";
            fl::string failure;
            failure.write(prefix, prefixLength);
            failure.write(error, sizeof(error) - 1);
            if (failure.size() == prefixLength + sizeof(error) - 1) {
                return failure;
            }
            return error;
        }
        char *bytes = &jsonStr[0]; // fl-lint: container-data-ptr-ok - contiguous mutable string for memmove/memcpy interop
        fl::memmove(bytes + prefixLength, bytes, bodyLength);
        fl::memcpy(bytes, prefix, prefixLength);
    }

    // Preserve prefix bytes; normalize only the serialized body in place.
    for (fl::size i = prefixLength; i < jsonStr.size(); ++i) {
        char &byte = jsonStr[i];
        if (byte == '\n' || byte == '\r') {
            byte = ' ';
        }
    }
    return jsonStr;
}

} // namespace fl
