#pragma once

// This file requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.
#ifdef FASTLED_HAS_NETWORKING

// Platform-specific socket includes (provides normalized POSIX API)
#ifdef FL_IS_WIN
    #include "platforms/win/socket_win.h"  // ok platform headers  // IWYU pragma: keep
#else
    #include "platforms/posix/socket_posix.h"  // ok platform headers  // IWYU pragma: keep
#endif

// Now include FastLED headers
#include "fl/stl/asio/http/native_client.h"

namespace fl {

NativeHttpClient::NativeHttpClient(const asio::ip::tcp::endpoint& ep, const ConnectionConfig& config)
    : mEndpoint(ep)
    , mConnection(config)
{
}

NativeHttpClient::NativeHttpClient(const string& host, u16 port, const ConnectionConfig& config)
    : mEndpoint(host, port)
    , mConnection(config)
{
}

NativeHttpClient::~NativeHttpClient() {
    disconnect();
}

bool NativeHttpClient::connect() {
    if (mConnection.getState() == ConnectionState::CLOSED) {
        return false;  // Permanently closed
    }

    // If already connected, return true
    if (isConnected()) {
        return true;
    }

    // Notify connection state machine
    mConnection.connect();

    // Attempt platform-specific connection
    if (platformConnect()) {
        mConnection.onConnected(0);  // TODO: pass actual currentTimeMs
        return true;
    }

    // Connection failed
    mConnection.onDisconnected();
    return false;
}

void NativeHttpClient::disconnect() {
    if (mSocket.is_open()) {
        platformDisconnect();
        mConnection.disconnect();
    }
}

void NativeHttpClient::close() {
    disconnect();
    mConnection.close();
}

bool NativeHttpClient::isConnected() const {
    return mConnection.isConnected() && isSocketConnected();
}

ConnectionState NativeHttpClient::getState() const {
    return mConnection.getState();
}

int NativeHttpClient::send(fl::span<const u8> data) {
    if (!isConnected() || !mSocket.is_open()) {
        return -1;
    }

    asio::error_code ec;
    size_t n = mSocket.write_some(data, ec);

    if (ec) {
        if (ec.code == asio::errc::would_block) {
            return 0;
        }
        mConnection.onDisconnected();
        return -1;
    }

    return static_cast<int>(n);
}

int NativeHttpClient::recv(fl::span<u8> buffer) {
    if (!isConnected() || !mSocket.is_open()) {
        return -1;
    }

    asio::error_code ec;
    size_t n = mSocket.read_some(buffer, ec);

    if (ec) {
        if (ec.code == asio::errc::would_block) {
            return 0;  // Non-blocking socket, no data available
        }
        // Connection error or EOF
        mConnection.onDisconnected();
        return -1;
    }

    // Data received, update heartbeat tracking
    mConnection.onHeartbeatReceived();

    return static_cast<int>(n);
}

void NativeHttpClient::update(u32 currentTimeMs) {
    // Update connection state machine
    mConnection.update(currentTimeMs);

    // Check if we should reconnect
    if (mConnection.shouldReconnect()) {
        // Attempt reconnection
        connect();
    }

    // Check if connection was lost
    if (!isConnected() && mSocket.is_open()) {
        disconnect();
    }
}

bool NativeHttpClient::shouldSendHeartbeat(u32 currentTimeMs) const {
    return mConnection.shouldSendHeartbeat(currentTimeMs);
}

void NativeHttpClient::onHeartbeatSent() {
    mConnection.onHeartbeatSent();
}

void NativeHttpClient::onHeartbeatReceived() {
    mConnection.onHeartbeatReceived();
}

u32 NativeHttpClient::getReconnectDelayMs() const {
    return mConnection.getReconnectDelayMs();
}

u32 NativeHttpClient::getReconnectAttempts() const {
    return mConnection.getReconnectAttempts();
}

bool NativeHttpClient::platformConnect() {
    // Delegate to tcp::socket::connect using the endpoint
    if (mSocket.is_open()) {
        platformDisconnect();
    }

    asio::error_code ec = mSocket.connect(mEndpoint);
    return ec.ok();
}

void NativeHttpClient::platformDisconnect() {
    mSocket.close();
}

bool NativeHttpClient::isSocketConnected() const {
    if (!mSocket.is_open()) {
        return false;
    }

    // Check socket status using getsockopt
    int error = 0;
    socklen_t len = sizeof(error);
    int ret = getsockopt(mSocket.native_handle(), SOL_SOCKET, SO_ERROR, (char*)&error, &len);

    if (ret != 0 || error != 0) {
        return false;
    }

    return true;
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
