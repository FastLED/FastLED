#include "fl/remote/rpc/server.h"

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
        return 0;
    }

    size_t processed = 0;

    // Pull JSON-RPC requests from source until none available
    while (auto optRequest = mRequestSource()) {
        fl::Json request = fl::move(*optRequest);

        // Process request through handler
        fl::Json response = mRequestHandler(request);

        // Queue response (skip scheduled acknowledgments - actual result sent later)
        bool isScheduledAck = response.contains("scheduled") && response["scheduled"].as_bool().value_or(false);
        if (!response.is_null() && !isScheduledAck) {
            mOutgoingQueue.push_back(fl::move(response));
        }

        processed++;
    }

    return processed;
}

size_t Server::push() {
    if (!mResponseSink) {
        return 0;
    }

    size_t sent = 0;

    // Push queued responses
    while (!mOutgoingQueue.empty()) {
        mResponseSink(mOutgoingQueue[0]);
        mOutgoingQueue.erase(mOutgoingQueue.begin());
        sent++;
    }

    return sent;
}

} // namespace fl
