#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/tuple.h"
#include "fl/stl/type_traits.h"

namespace fl {

// Forward declaration
class ResponseSend;

// =============================================================================
// ResponseAware Traits - Detect and strip ResponseSend& from function signatures
// =============================================================================

/**
 * @brief Type trait to detect if a function signature has ResponseSend& as first parameter
 *
 * This trait is used to distinguish between:
 * - Regular RPC methods: int(int, int)
 * - Response-aware RPC methods: void(ResponseSend&, int, int)
 *
 * For response-aware methods, the ResponseSend& parameter is stripped from the
 * signature for JSON parameter matching:
 * - Function signature: void(ResponseSend&, int, int)
 * - JSON params: [int, int]
 *
 * EXAMPLE:
 *
 *   // Regular method
 *   int add(int a, int b) { return a + b; }
 *   // Signature for JSON matching: int(int, int)
 *
 *   // Response-aware async method
 *   void addAsync(ResponseSend& send, int a, int b) {
 *       send.send(Json::object().set("result", a + b));
 *   }
 *   // Signature for JSON matching: void(int, int)  <- ResponseSend& stripped
 */

// =============================================================================
// Primary template: Not response-aware
// =============================================================================

template<typename Sig>
struct response_aware_signature {
    static constexpr bool is_response_aware = false;
    using signature = Sig;  // Original signature
};

// =============================================================================
// Specialization for response-aware signatures: R(ResponseSend&, Args...)
// =============================================================================

template<typename R, typename... Args>
struct response_aware_signature<R(ResponseSend&, Args...)> {
    static constexpr bool is_response_aware = true;
    using signature = R(Args...);  // Stripped signature (without ResponseSend&)
    using full_signature = R(ResponseSend&, Args...);  // Original signature
};

// =============================================================================
// Specialization for function pointers: R(*)(ResponseSend&, Args...)
// =============================================================================

template<typename R, typename... Args>
struct response_aware_signature<R(*)(ResponseSend&, Args...)> {
    static constexpr bool is_response_aware = true;
    using signature = R(Args...);  // Stripped signature
    using full_signature = R(ResponseSend&, Args...);  // Original signature
};

// =============================================================================
// Helper to check if a type is ResponseSend (any cv-qualification or reference)
// =============================================================================

template<typename T>
struct is_response_send : fl::false_type {};

template<>
struct is_response_send<ResponseSend> : fl::true_type {};

template<>
struct is_response_send<ResponseSend&> : fl::true_type {};

template<>
struct is_response_send<const ResponseSend&> : fl::true_type {};

template<>
struct is_response_send<ResponseSend&&> : fl::true_type {};

} // namespace fl
