#pragma once

#include "fl/net/http/stream_transport.h"
#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace net {
namespace http {

// --- StreamHandle implementation ---

StreamHandle::StreamHandle(fl::task::Promise<fl::json> p,
                           fl::shared_ptr<fl::function<void(const fl::json&)>> updateCb)
    : mPromise(fl::move(p))
    , mUpdateCallback(fl::move(updateCb)) {
}

StreamHandle& StreamHandle::onData(fl::function<void(const fl::json&)> cb) {
    if (mUpdateCallback) {
        *mUpdateCallback = fl::move(cb);
    }
    return *this;
}

StreamHandle& StreamHandle::then(fl::function<void(const fl::json&)> cb) {
    mPromise.then(fl::move(cb));
    return *this;
}

StreamHandle& StreamHandle::catch_(fl::function<void(const fl::task::Error&)> cb) {
    mPromise.catch_(fl::move(cb));
    return *this;
}

fl::task::Promise<fl::json>& StreamHandle::promise() {
    return mPromise;
}

bool StreamHandle::valid() const {
    return mPromise.valid();
}

// --- HttpStreamTransport implementation ---

HttpStreamTransport::HttpStreamTransport(const fl::string& host, u16 port, u32 heartbeatIntervalMs)
    : mConnection(ConnectionConfig{})  // Use default config
    , mLastHeartbeatSent(0)
    , mLastHeartbeatReceived(0)
    , mHeartbeatInterval(heartbeatIntervalMs)
    , mTimeoutMs(60000)  // Default 60s timeout
    , mWasConnected(false) {
}

HttpStreamTransport::~HttpStreamTransport() FL_NOEXCEPT {
    // Note: Cannot call pure virtual disconnect() here
    // Subclasses must clean up in their own destructors
}

fl::string HttpStreamTransport::idToString(const fl::json& id) {
    if (id.is_int()) {
        char buf[32];
        fl::snprintf(buf, sizeof(buf), "%d", id.as_int().value());
        return fl::string(buf);
    }
    if (id.is_string()) {
        return id.as_string().value();
    }
    // Fallback for other types - use string representation
    return id.to_string();
}

void HttpStreamTransport::parseChunkedMessages() {
    for (;;) {
        size_t chunkSize = mReader.nextChunkSize();
        if (chunkSize == 0) {
            break;
        }

        u8 stackBuf[512];
        fl::vector<u8> heapBuf;
        fl::span<u8> outSpan;
        if (chunkSize <= sizeof(stackBuf)) {
            outSpan = fl::span<u8>(stackBuf, sizeof(stackBuf));
        } else {
            heapBuf.resize(chunkSize);
            outSpan = heapBuf;
        }

        ChunkedReadResult result = mReader.readChunk(outSpan);
        if (!result.hasData()) {
            break;
        }

        fl::string jsonStr(reinterpret_cast<const char*>(result.mData.data()), result.mData.size()); // ok reinterpret cast
        fl::json json = fl::json::parse(jsonStr.c_str());
        if (json.is_null()) {
            continue;
        }

        // Filter heartbeats
        if (json["method"].as_string() == "rpc.ping") {
            mLastHeartbeatReceived = getCurrentTimeMs();
            continue;
        }

        mLastHeartbeatReceived = getCurrentTimeMs();

        // Try to dispatch to pending calls/streams by id
        if (json.contains("id") && !json["id"].is_null()) {
            fl::string idKey = idToString(json["id"]);

            if (resolveRpc(json, idKey)) {
                continue;  // Consumed by pending call
            }
            if (resolveRpcStream(json, idKey)) {
                continue;  // Consumed by pending stream
            }
        }

        // Not matched - add to incoming queue for readRequest()
        mIncomingQueue.push_back(fl::move(json));
    }
}

bool HttpStreamTransport::resolveRpc(const fl::json& msg, const fl::string& idKey) {
    auto it = mPendingCalls.find(idKey);
    if (it == mPendingCalls.end()) {
        return false;
    }
    PendingCall* pending = &it->second;

    // Check for error
    if (msg.contains("error")) {
        fl::task::Error err(msg["error"]["message"].as_string().value());
        pending->promise.complete_with_error(err);
        mPendingCalls.erase(idKey);
        return true;
    }

    // Check for ACK (ASYNC mode sends acknowledged first)
    if (msg.contains("result") && msg["result"].contains("acknowledged")) {
        if (msg["result"]["acknowledged"].as_bool() == true) {
            pending->ackReceived = true;
            return true;  // Stay pending, wait for final result
        }
    }

    // Final result
    pending->promise.complete_with_value(msg);
    mPendingCalls.erase(idKey);
    return true;
}

bool HttpStreamTransport::resolveRpcStream(const fl::json& msg, const fl::string& idKey) {
    auto it = mPendingStreams.find(idKey);
    if (it == mPendingStreams.end()) {
        return false;
    }
    PendingStream* pending = &it->second;

    // Check for error
    if (msg.contains("error")) {
        fl::task::Error err(msg["error"]["message"].as_string().value());
        pending->promise.complete_with_error(err);
        mPendingStreams.erase(idKey);
        return true;
    }

    // Check for ACK
    if (msg.contains("result") && msg["result"].contains("acknowledged")) {
        if (msg["result"]["acknowledged"].as_bool() == true) {
            pending->ackReceived = true;
            return true;
        }
    }

    // Check for stream update
    if (msg.contains("result") && msg["result"].contains("update")) {
        if (pending->updateCallback && *pending->updateCallback) {
            (*pending->updateCallback)(msg["result"]["update"]);
        }
        return true;
    }

    // Check for stream final (stop marker)
    if (msg.contains("result") && msg["result"].contains("stop")) {
        if (msg["result"]["stop"].as_bool() == true) {
            // Resolve with the value field if present, otherwise the full result
            if (msg["result"].contains("value")) {
                pending->promise.complete_with_value(msg["result"]["value"]);
            } else {
                pending->promise.complete_with_value(msg["result"]);
            }
            mPendingStreams.erase(idKey);
            return true;
        }
    }

    // Unrecognized message shape - treat as final
    pending->promise.complete_with_value(msg);
    mPendingStreams.erase(idKey);
    return true;
}

fl::task::Promise<fl::json> HttpStreamTransport::rpc(const fl::string& method, const fl::json& params) {
    int id = mNextCallId++;
    fl::json request = fl::json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", method);
    request.set("params", params);
    request.set("id", id);
    return rpc(request);
}

fl::task::Promise<fl::json> HttpStreamTransport::rpc(const fl::json& fullRequest) {
    fl::task::Promise<fl::json> p = fl::task::Promise<fl::json>::create();

    if (!isConnected()) {
        p.complete_with_error(fl::task::Error("Not connected"));
        return p;
    }

    fl::string idKey = idToString(fullRequest["id"]);
    PendingCall pc;
    pc.promise = p;
    mPendingCalls.insert(idKey, fl::move(pc));

    writeResponse(fullRequest);
    return p;
}

StreamHandle HttpStreamTransport::rpcStream(const fl::string& method, const fl::json& params) {
    int id = mNextCallId++;
    fl::json request = fl::json::object();
    request.set("jsonrpc", "2.0");
    request.set("method", method);
    request.set("params", params);
    request.set("id", id);
    return rpcStream(request);
}

StreamHandle HttpStreamTransport::rpcStream(const fl::json& fullRequest) {
    auto updateCb = fl::make_shared<fl::function<void(const fl::json&)>>();
    fl::task::Promise<fl::json> p = fl::task::Promise<fl::json>::create();

    if (!isConnected()) {
        p.complete_with_error(fl::task::Error("Not connected"));
        return StreamHandle(fl::move(p), fl::move(updateCb));
    }

    fl::string idKey = idToString(fullRequest["id"]);
    PendingStream ps;
    ps.promise = p;
    ps.updateCallback = updateCb;
    mPendingStreams.insert(idKey, fl::move(ps));

    writeResponse(fullRequest);
    return StreamHandle(p, updateCb);
}

fl::optional<fl::json> HttpStreamTransport::readRequest() {
    if (!isConnected()) {
        return fl::nullopt;
    }

    // Process incoming data and drain into queue
    processIncomingData();
    parseChunkedMessages();

    // Pop from incoming queue
    if (mIncomingQueue.empty()) {
        return fl::nullopt;
    }

    fl::json front = fl::move(mIncomingQueue[0]);
    // Shift remaining elements
    fl::vector<fl::json> remaining;
    remaining.reserve(mIncomingQueue.size() - 1);
    for (size_t i = 1; i < mIncomingQueue.size(); i++) {
        remaining.push_back(fl::move(mIncomingQueue[i]));
    }
    mIncomingQueue = fl::move(remaining);

    return front;
}

void HttpStreamTransport::writeResponse(const fl::json& response) {
    if (!isConnected()) {
        return;
    }

    // Serialize JSON to string
    fl::string jsonStr = response.to_string();

    // Write chunked data into stack buffer (or heap for large payloads)
    size_t needed = ChunkedWriter::chunkOverhead(jsonStr.size());
    u8 stackBuf[512];
    fl::vector<u8> heapBuf;
    fl::span<u8> outSpan;
    if (needed <= sizeof(stackBuf)) {
        outSpan = fl::span<u8>(stackBuf, sizeof(stackBuf));
    } else {
        heapBuf.resize(needed);
        outSpan = heapBuf;
    }
    size_t written = mWriter.writeChunk(
        fl::span<const u8>(reinterpret_cast<const u8*>(jsonStr.c_str()), jsonStr.size()), // ok reinterpret cast
        outSpan
    );

    // Send chunked data
    if (written > 0) {
        sendData(fl::span<const u8>(outSpan.data(), written));
    }

    // Update last sent time
    mLastHeartbeatSent = getCurrentTimeMs();
}

void HttpStreamTransport::update(u32 currentTimeMs) {
    // Update connection state
    mConnection.update(currentTimeMs);
    bool nowConnected = isConnected();

    // Handle state changes
    if (mWasConnected != nowConnected) {
        handleConnectionStateChange(currentTimeMs);
        mWasConnected = nowConnected;
    }

    if (!isConnected()) {
        // Attempt reconnection if needed
        if (mConnection.shouldReconnect()) {
            triggerReconnect();
        }
        return;
    }

    // Send heartbeat if needed
    u32 timeSinceLastSent = currentTimeMs - mLastHeartbeatSent;
    if (timeSinceLastSent >= mHeartbeatInterval) {
        sendHeartbeat();
    }

    // Check for heartbeat timeout
    checkHeartbeatTimeout(currentTimeMs);

    // Process incoming data and drain messages (resolves promises)
    processIncomingData();
    parseChunkedMessages();
}

void HttpStreamTransport::setOnConnect(StateCallback callback) {
    mOnConnect = callback;
}

void HttpStreamTransport::setOnDisconnect(StateCallback callback) {
    mOnDisconnect = callback;
}

void HttpStreamTransport::setHeartbeatInterval(u32 intervalMs) {
    mHeartbeatInterval = intervalMs;
}

u32 HttpStreamTransport::getHeartbeatInterval() const {
    return mHeartbeatInterval;
}

void HttpStreamTransport::setTimeout(u32 timeoutMs) {
    mTimeoutMs = timeoutMs;
}

u32 HttpStreamTransport::getTimeout() const {
    return mTimeoutMs;
}

u32 HttpStreamTransport::getCurrentTimeMs() const {
    // Default implementation uses system time
    // Subclasses can override for testing
    return fl::millis();
}

void HttpStreamTransport::triggerReconnect() {
    // Default implementation does nothing
    // Subclasses should override to implement reconnection logic
}

void HttpStreamTransport::sendHeartbeat() {
    if (!isConnected()) {
        return;
    }

    // Create heartbeat request (rpc.ping notification)
    fl::json heartbeat = fl::json::object();
    heartbeat.set("jsonrpc", "2.0");
    heartbeat.set("method", "rpc.ping");
    heartbeat.set("id", fl::json());  // null value

    // Write as response (uses same chunked encoding)
    writeResponse(heartbeat);
}

void HttpStreamTransport::checkHeartbeatTimeout(u32 currentTimeMs) {
    u32 timeSinceLastReceived = currentTimeMs - mLastHeartbeatReceived;
    if (timeSinceLastReceived >= getTimeout()) {
        // Heartbeat timeout - connection is dead
        mConnection.onDisconnected();
        disconnect();
    }
}

bool HttpStreamTransport::processIncomingData() {
    if (!isConnected()) {
        return false;
    }

    // Read incoming data from socket
    u8 buffer[1024];
    int bytesRead = recvData(buffer);

    if (bytesRead < 0) {
        // Error reading from socket
        mConnection.onDisconnected();
        disconnect();
        return false;
    }

    if (bytesRead == 0) {
        // No data available (non-blocking socket)
        return false;
    }

    // Feed data to chunked reader
    mReader.feed(fl::span<const u8>(buffer, static_cast<size_t>(bytesRead)));

    return true;
}

void HttpStreamTransport::handleConnectionStateChange(u32 currentTimeMs) {
    if (isConnected()) {
        // Just connected — use the caller-provided timestamp to avoid
        // unsigned-underflow races between getCurrentTimeMs() and the
        // currentTimeMs snapshot used by checkHeartbeatTimeout().
        mLastHeartbeatSent = currentTimeMs;
        mLastHeartbeatReceived = currentTimeMs;
        if (mOnConnect) {
            mOnConnect();
        }
    } else {
        // Just disconnected
        if (mOnDisconnect) {
            mOnDisconnect();
        }
    }
}

}  // namespace http
}  // namespace net
}  // namespace fl
