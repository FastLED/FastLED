#pragma once

#include "fl/stl/json.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief Result metadata for executed RPC calls
 *
 * Contains execution timing information and return value.
 */
struct RpcResult {
    fl::string functionName;    // Name of function that executed
    fl::json result;            // Return value (null if no return)
    u32 scheduledAt;            // Timestamp when scheduled (0 for immediate)
    u32 receivedAt;             // Timestamp when RPC request received
    u32 executedAt;             // Timestamp when function executed
    bool wasScheduled;          // true if scheduled, false if immediate

    /**
     * @brief Serialize result to JSON object (compact single-line format)
     * @return JSON object with all result fields
     */
    fl::json to_json() const FL_NO_EXCEPT;
};

/**
 * @brief Flags for clearing Remote state (can be OR'd together)
 */
enum class RemoteClearFlags : u8 {
    None = 0,
    Results = 1 << 0,      ///< Clear results from executed functions
    Scheduled = 1 << 1,    ///< Clear scheduled calls
    Functions = 1 << 2,    ///< Clear registered functions
    All = Results | Scheduled | Functions  ///< Clear everything
};

// Bitwise operators for RemoteClearFlags
inline RemoteClearFlags operator|(RemoteClearFlags a, RemoteClearFlags b) FL_NO_EXCEPT {
    return static_cast<RemoteClearFlags>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline RemoteClearFlags operator&(RemoteClearFlags a, RemoteClearFlags b) FL_NO_EXCEPT {
    return static_cast<RemoteClearFlags>(static_cast<u8>(a) & static_cast<u8>(b));
}

inline bool operator!(RemoteClearFlags flags) FL_NO_EXCEPT {
    return static_cast<u8>(flags) == 0;
}

} // namespace fl
