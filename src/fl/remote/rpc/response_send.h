#pragma once

#include "fl/stl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief Helper class for sending responses in async/streaming RPC methods
 *
 * ResponseSend provides a simple API for async RPC methods to send responses:
 * - send(): Send a single response (for ASYNC mode)
 * - sendUpdate(): Send intermediate streaming updates (for ASYNC_STREAM mode)
 * - sendFinal(): Send final response and mark stream as complete (for ASYNC_STREAM mode)
 *
 * The request ID is automatically attached to all responses.
 *
 * EXAMPLE USAGE (ASYNC mode):
 *
 *   remote.bindAsync("longTask", [](ResponseSend& send, int param) {
 *       // ACK is already sent automatically
 *
 *       // Do work...
 *       int result = doLongWork(param);
 *
 *       // Send final result
 *       send.send(json::object().set("value", result));
 *   });
 *
 * EXAMPLE USAGE (ASYNC_STREAM mode):
 *
 *   remote.bindAsync("streamData", [](ResponseSend& send, int count) {
 *       // ACK is already sent automatically
 *
 *       // Send multiple updates
 *       for (int i = 0; i < count; i++) {
 *           send.sendUpdate(json::object().set("progress", i * 10));
 *       }
 *
 *       // Send final result
 *       send.sendFinal(json::object().set("done", true));
 *   }, RpcMode::ASYNC_STREAM);
 */
class ResponseSend {
public:
    /**
     * @brief Construct ResponseSend with request ID and response sink
     * @param requestId The JSON-RPC request ID (can be int, string, or null)
     * @param sink Function to send JSON responses
     */
    ResponseSend(const fl::json& requestId, fl::function<void(const fl::json&)> sink)
        : mRequestId(requestId), mResponseSink(fl::move(sink)), mIsFinal(false) {}

    // Non-copyable but movable
    ResponseSend(const ResponseSend&) FL_NOEXCEPT = delete;
    ResponseSend& operator=(const ResponseSend&) FL_NOEXCEPT = delete;
    ResponseSend(ResponseSend&&) FL_NOEXCEPT = default;
    ResponseSend& operator=(ResponseSend&&) FL_NOEXCEPT = default;

    /**
     * @brief Send a single response (for ASYNC mode)
     * @param result The result payload (any JSON value)
     *
     * Creates a JSON-RPC response:
     * {"jsonrpc": "2.0", "result": <result>, "id": <requestId>}
     */
    void send(const fl::json& result) {
        if (!mResponseSink || mIsFinal) {
            return;
        }

        fl::json response = fl::json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", result);
        response.set("id", mRequestId);

        mResponseSink(response);
    }

    /**
     * @brief Send intermediate streaming update (for ASYNC_STREAM mode)
     * @param update The update payload (any JSON value)
     *
     * Creates a JSON-RPC response with "update" marker:
     * {"jsonrpc": "2.0", "result": {"update": <update>}, "id": <requestId>}
     */
    void sendUpdate(const fl::json& update) {
        if (!mResponseSink || mIsFinal) {
            return;
        }

        fl::json response = fl::json::object();
        response.set("jsonrpc", "2.0");

        fl::json result = fl::json::object();
        result.set("update", update);

        response.set("result", result);
        response.set("id", mRequestId);

        mResponseSink(response);
    }

    /**
     * @brief Send final response and mark stream as complete (for ASYNC_STREAM mode)
     * @param result The final result payload (any JSON value)
     *
     * Creates a JSON-RPC response with "stop" marker:
     * {"jsonrpc": "2.0", "result": {"value": <result>, "stop": true}, "id": <requestId>}
     *
     * After calling sendFinal(), no more responses can be sent.
     */
    void sendFinal(const fl::json& result) {
        if (!mResponseSink || mIsFinal) {
            return;
        }

        mIsFinal = true;

        fl::json response = fl::json::object();
        response.set("jsonrpc", "2.0");

        fl::json finalResult = fl::json::object();
        finalResult.set("value", result);
        finalResult.set("stop", true);

        response.set("result", finalResult);
        response.set("id", mRequestId);

        mResponseSink(response);
    }

    /**
     * @brief Check if final response has been sent
     * @return true if sendFinal() was called, false otherwise
     */
    bool isFinal() const {
        return mIsFinal;
    }

    /**
     * @brief Get the request ID
     * @return The JSON-RPC request ID
     */
    const fl::json& requestId() const {
        return mRequestId;
    }

private:
    fl::json mRequestId;                          // Request ID from JSON-RPC request
    fl::function<void(const fl::json&)> mResponseSink;  // Function to send responses
    bool mIsFinal;                                // True if sendFinal() was called
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
