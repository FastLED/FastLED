// src/fl/remote/transport/serial.cpp.hpp
// Implementation of JSON-RPC serial transport layer

#pragma once

#include "serial.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/cstring.h"
#include "fl/stl/strstream.h"

namespace fl {

fl::string formatJsonResponse(const fl::Json& response, const char* prefix) {
    // Use sstream to minimize allocations
    fl::sstream ss;

    // Add prefix if provided
    if (prefix && prefix[0] != '\0') {
        ss << prefix;
    }

    // Serialize to compact JSON
    fl::string jsonStr = response.to_string();

    // Stream copy with inline filtering (replace newlines with spaces)
    for (size_t i = 0; i < jsonStr.size(); ++i) {
        char c = jsonStr[i];
        ss << ((c == '\n' || c == '\r') ? ' ' : c);
    }

    return ss.str();
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
