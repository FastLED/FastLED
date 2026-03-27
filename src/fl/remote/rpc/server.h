#pragma once

#include "fl/stl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/function.h"
#include "fl/stl/optional.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief JSON-RPC server with callback-based I/O
 *
 * Coordinates JSON-RPC request/response flow between source and sink callbacks.
 * Provides generic I/O coordination layer for JSON-RPC servers.
 *
 * Usage:
 *   Server server(
 *       []() { return getJsonRpcRequest(); },      // RequestSource
 *       [](const json& r) { sendJsonRpcResponse(r); } // ResponseSink
 *   );
 *   server.setRequestHandler([](const json& req) {
 *       return processJsonRpc(req);
 *   });
 *   server.update();  // pull + push
 */
class Server {
public:
    using RequestSource = fl::function<fl::optional<fl::json>()>;
    using ResponseSink = fl::function<void(const fl::json&)>;
    using RequestHandler = fl::function<fl::json(const fl::json&)>;

    /**
     * @brief Default constructor
     *
     * Creates Server with no-op callbacks. Use setRequestSource/setResponseSink
     * to configure I/O, or call setRequestHandler() manually.
     */
    Server() FL_NOEXCEPT;

    /**
     * @brief Construct with I/O callbacks
     * @param source Function that returns next JSON-RPC request (or nullopt if none)
     * @param sink Function that handles outgoing JSON-RPC responses
     */
    Server(RequestSource source, ResponseSink sink);

    virtual ~Server() FL_NOEXCEPT = default;

    // Non-copyable but movable
    Server(const Server&) FL_NOEXCEPT = delete;
    Server& operator=(const Server&) FL_NOEXCEPT = delete;
    Server(Server&&) FL_NOEXCEPT = default;
    Server& operator=(Server&&) FL_NOEXCEPT = default;

    /**
     * @brief Set request handler
     * @param handler Function that processes JSON-RPC requests and returns responses
     */
    void setRequestHandler(RequestHandler handler);

    /**
     * @brief Set request source callback
     */
    void setRequestSource(RequestSource source);

    /**
     * @brief Set response sink callback
     */
    void setResponseSink(ResponseSink sink);

    /**
     * @brief Main update: pull + push
     */
    size_t update();

    /**
     * @brief Pull requests from source, process, queue responses
     */
    size_t pull();

    /**
     * @brief Push queued responses to sink
     */
    size_t push();

protected:
    RequestSource mRequestSource;
    ResponseSink mResponseSink;
    RequestHandler mRequestHandler;
    fl::vector<fl::json> mOutgoingQueue;
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
