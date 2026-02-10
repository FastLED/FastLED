#pragma once

#include "fl/json.h"

#include "fl/stl/function.h"
#include "fl/stl/optional.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"

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
 *       [](const Json& r) { sendJsonRpcResponse(r); } // ResponseSink
 *   );
 *   server.setRequestHandler([](const Json& req) {
 *       return processJsonRpc(req);
 *   });
 *   server.update();  // pull + push
 */
class Server {
public:
    using RequestSource = fl::function<fl::optional<fl::Json>()>;
    using ResponseSink = fl::function<void(const fl::Json&)>;
    using RequestHandler = fl::function<fl::Json(const fl::Json&)>;

    /**
     * @brief Default constructor
     *
     * Creates Server with no-op callbacks. Use setRequestSource/setResponseSink
     * to configure I/O, or call setRequestHandler() manually.
     */
    Server();

    /**
     * @brief Construct with I/O callbacks
     * @param source Function that returns next JSON-RPC request (or nullopt if none)
     * @param sink Function that handles outgoing JSON-RPC responses
     */
    Server(RequestSource source, ResponseSink sink);

    virtual ~Server() = default;

    // Non-copyable but movable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = default;
    Server& operator=(Server&&) = default;

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
    fl::vector<fl::Json> mOutgoingQueue;
};

} // namespace fl
