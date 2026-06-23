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
#include "fl/stl/asio/http/native_server.h"
#include "fl/stl/noexcept.h"

namespace fl {

NativeHttpServer::NativeHttpServer(u16 port, const ConnectionConfig& config)
    : mPort(port)
    , mIsListening(false)
    , mNextClientId(1)
    , mConfig(config)
{
}

NativeHttpServer::~NativeHttpServer() FL_NOEXCEPT {
    stop();
}

bool NativeHttpServer::start() {
    if (mIsListening) {
        return true;  // Already listening
    }

    if (platformStartListening()) {
        mIsListening = true;
        return true;
    }

    return false;
}

void NativeHttpServer::stop() {
    // Disconnect all clients
    disconnectAllClients();

    // Stop listening
    platformStopListening();
    mIsListening = false;
}

bool NativeHttpServer::isListening() const {
    return mIsListening;
}

void NativeHttpServer::acceptClients() {
    if (!mIsListening) {
        return;
    }

    // Accept new clients in a loop (non-blocking)
    while (true) {
        ServerClientConnection conn;
        conn.clientId = mNextClientId;
        conn.connection = HttpConnection(mConfig);

        asio::error_code ec = mAcceptor.accept(conn.socket);
        if (ec) {
            break;  // No more clients to accept (would_block) or error
        }

        conn.connection.onConnected(0);  // Mark as connected immediately
        mNextClientId++;
        mClients.push_back(fl::move(conn));
    }
}

size_t NativeHttpServer::getClientCount() const {
    return mClients.size();
}

bool NativeHttpServer::hasClient(u32 clientId) const {
    return findClient(clientId) != nullptr;
}

void NativeHttpServer::disconnectClient(u32 clientId) {
    removeClient(clientId);
}

void NativeHttpServer::disconnectAllClients() {
    // tcp::socket closes automatically in destructor
    mClients.clear();
}

int NativeHttpServer::send(u32 clientId, fl::span<const u8> data) {
    ServerClientConnection* client = findClient(clientId);
    if (!client || !client->socket.is_open()) {
        return -1;
    }

    asio::error_code ec;
    size_t n = client->socket.write_some(data, ec);

    if (ec) {
        if (ec.code == asio::errc::would_block) {
            return 0;
        }
        // Connection error, disconnect client
        client->connection.onDisconnected();
        return -1;
    }

    return static_cast<int>(n);
}

int NativeHttpServer::recv(u32 clientId, fl::span<u8> buffer) {
    ServerClientConnection* client = findClient(clientId);
    if (!client || !client->socket.is_open()) {
        return -1;
    }

    asio::error_code ec;
    size_t n = client->socket.read_some(buffer, ec);

    if (ec) {
        if (ec.code == asio::errc::would_block) {
            return 0;  // Non-blocking socket, no data available
        }
        // Connection error or EOF, disconnect client
        client->connection.onDisconnected();
        return -1;
    }

    // Data received, update heartbeat tracking
    client->connection.onHeartbeatReceived();

    return static_cast<int>(n);
}

void NativeHttpServer::broadcast(fl::span<const u8> data) {
    // Send to all clients
    for (auto& client : mClients) {
        send(client.clientId, data);
    }
}

void NativeHttpServer::update(u32 currentTimeMs) {
    // Check for dead connections
    for (size_t i = 0; i < mClients.size(); ) {
        auto& client = mClients[i];

        // Update connection state
        client.connection.update(currentTimeMs);

        // Check if client is still connected
        if (!client.connection.isConnected() || !isSocketConnected(client.socket)) {
            // Remove disconnected client (socket closes in destructor)
            mClients.erase(mClients.begin() + i);
            continue;
        }

        ++i;
    }
}

fl::vector<u32> NativeHttpServer::getClientIds() const {
    fl::vector<u32> ids;
    ids.reserve(mClients.size());
    for (const auto& client : mClients) {
        ids.push_back(client.clientId);
    }
    return ids;
}

bool NativeHttpServer::platformStartListening() {
    asio::error_code ec = mAcceptor.open(mPort);
    if (ec) {
        return false;
    }

    // Update port in case 0 was requested
    mPort = mAcceptor.port();

    ec = mAcceptor.listen(10);
    if (ec) {
        mAcceptor.close();
        return false;
    }

    return true;
}

void NativeHttpServer::platformStopListening() {
    mAcceptor.close();
}

ServerClientConnection* NativeHttpServer::findClient(u32 clientId) {
    for (auto& client : mClients) {
        if (client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

const ServerClientConnection* NativeHttpServer::findClient(u32 clientId) const {
    for (const auto& client : mClients) {
        if (client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

void NativeHttpServer::removeClient(u32 clientId) {
    for (size_t i = 0; i < mClients.size(); ++i) {
        if (mClients[i].clientId == clientId) {
            // Socket closes automatically in destructor
            mClients.erase(mClients.begin() + i);
            return;
        }
    }
}

bool NativeHttpServer::isSocketConnected(const asio::ip::tcp::socket& sock) const {
    if (!sock.is_open()) {
        return false;
    }

    // Check socket status using getsockopt
    int error = 0;
    socklen_t len = sizeof(error);
    int ret = getsockopt(sock.native_handle(), SOL_SOCKET, SO_ERROR, (char*)&error, &len);

    if (ret != 0 || error != 0) {
        return false;
    }

    return true;
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
