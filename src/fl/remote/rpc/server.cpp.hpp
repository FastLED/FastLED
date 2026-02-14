#include "fl/remote/rpc/server.h"

#if FASTLED_ENABLE_JSON

#include "fl/json.h"
#include "fl/log.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/optional.h"
#include "fl/stl/strstream.h"
#include "fl/stl/vector.h"

namespace fl {

Server::Server()
    : mRequestSource([]() { return fl::nullopt; })
    , mResponseSink([](const fl::Json&) {})
{}

Server::Server(RequestSource source, ResponseSink sink)
    : mRequestSource(fl::move(source))
    , mResponseSink(fl::move(sink))
{}

void Server::setRequestHandler(RequestHandler handler) {
    mRequestHandler = fl::move(handler);
}

void Server::setRequestSource(RequestSource source) {
    mRequestSource = fl::move(source);
}

void Server::setResponseSink(ResponseSink sink) {
    mResponseSink = fl::move(sink);
}

size_t Server::update() {
    size_t processed = pull();
    size_t sent = push();
    return processed + sent;
}

size_t Server::pull() {
    if (!mRequestSource || !mRequestHandler) {
        FL_WARN("[RPC DEBUG] pull(): No request source or handler");
        return 0;
    }

    size_t processed = 0;

    // Pull JSON-RPC requests from source until none available
    while (auto optRequest = mRequestSource()) {
        fl::Json request = fl::move(*optRequest);
        FL_WARN("[RPC DEBUG] pull(): Received request");

        // Process request through handler
        fl::Json response = mRequestHandler(request);
        FL_WARN("[RPC DEBUG] pull(): Got response, is_null=" << response.is_null());

        // Queue response (skip scheduled acknowledgments - actual result sent later)
        bool isScheduledAck = response.contains("scheduled") && response["scheduled"].as_bool().value_or(false);
        if (!response.is_null() && !isScheduledAck) {
            FL_WARN("[RPC DEBUG] pull(): Queueing response (queue size before: " << mOutgoingQueue.size() << ")");
            mOutgoingQueue.push_back(fl::move(response));
            FL_WARN("[RPC DEBUG] pull(): Response queued (queue size after: " << mOutgoingQueue.size() << ")");
        } else {
            FL_WARN("[RPC DEBUG] pull(): NOT queueing response (is_null=" << response.is_null() << ", isScheduledAck=" << isScheduledAck << ")");
        }

        processed++;
    }

    FL_WARN("[RPC DEBUG] pull(): Processed " << processed << " requests, queue size: " << mOutgoingQueue.size());
    return processed;
}

size_t Server::push() {
    if (!mResponseSink) {
        FL_WARN("[RPC DEBUG] push(): No response sink configured!");
        return 0;
    }

    FL_WARN("[RPC DEBUG] push(): Starting, queue size: " << mOutgoingQueue.size());
    size_t sent = 0;

    // Push queued responses
    while (!mOutgoingQueue.empty()) {
        FL_WARN("[RPC DEBUG] push(): Sending response #" << (sent + 1));
        mResponseSink(mOutgoingQueue[0]);
        mOutgoingQueue.erase(mOutgoingQueue.begin());
        sent++;
        FL_WARN("[RPC DEBUG] push(): Response sent, remaining in queue: " << mOutgoingQueue.size());
    }

    FL_WARN("[RPC DEBUG] push(): Completed, sent " << sent << " responses");
    return sent;
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
