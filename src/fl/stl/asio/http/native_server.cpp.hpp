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
    FL_NO_EXCEPT : mPort(port)
    , mIsListening(false)
    , mNextClientId(1)
    , mConfig(config)
{
}

NativeHttpServer::~NativeHttpServer() FL_NO_EXCEPT {
    stop();
}

bool NativeHttpServer::start() FL_NO_EXCEPT {
    if (mIsListening) {
        return true;  // Already listening
    }

    if (platformStartListening()) {
        mIsListening = true;
        return true;
    }

    return false;
}

void NativeHttpServer::stop() FL_NO_EXCEPT {
    // Disconnect all clients
    disconnectAllClients();

    // Stop listening
    platformStopListening();
    mIsListening = false;
}

bool NativeHttpServer::isListening() const FL_NO_EXCEPT {
    return mIsListening;
}

void NativeHttpServer::acceptClients() FL_NO_EXCEPT {
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

size_t NativeHttpServer::getClientCount() const FL_NO_EXCEPT {
    return mClients.size();
}

bool NativeHttpServer::hasClient(u32 clientId) const FL_NO_EXCEPT {
    return findClient(clientId) != nullptr;
}

void NativeHttpServer::disconnectClient(u32 clientId) FL_NO_EXCEPT {
    removeClient(clientId);
}

void NativeHttpServer::disconnectAllClients() FL_NO_EXCEPT {
    // tcp::socket closes automatically in destructor
    mClients.clear();
}

int NativeHttpServer::send(u32 clientId, fl::span<const u8> data) FL_NO_EXCEPT {
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

int NativeHttpServer::recv(u32 clientId, fl::span<u8> buffer) FL_NO_EXCEPT {
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

void NativeHttpServer::broadcast(fl::span<const u8> data) FL_NO_EXCEPT {
    // Send to all clients
    for (auto& client : mClients) {
        send(client.clientId, data);
    }
}

void NativeHttpServer::update(u32 currentTimeMs) FL_NO_EXCEPT {
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

fl::vector<u32> NativeHttpServer::getClientIds() const FL_NO_EXCEPT {
    fl::vector<u32> ids;
    ids.reserve(mClients.size());
    for (const auto& client : mClients) {
        ids.push_back(client.clientId);
    }
    return ids;
}

bool NativeHttpServer::platformStartListening() FL_NO_EXCEPT {
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

void NativeHttpServer::platformStopListening() FL_NO_EXCEPT {
    mAcceptor.close();
}

ServerClientConnection* NativeHttpServer::findClient(u32 clientId) FL_NO_EXCEPT {
    for (auto& client : mClients) {
        if (client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

const ServerClientConnection* NativeHttpServer::findClient(u32 clientId) const FL_NO_EXCEPT {
    for (const auto& client : mClients) {
        if (client.clientId == clientId) {
            return &client;
        }
    }
    return nullptr;
}

void NativeHttpServer::removeClient(u32 clientId) FL_NO_EXCEPT {
    for (size_t i = 0; i < mClients.size(); ++i) {
        if (mClients[i].clientId == clientId) {
            // Socket closes automatically in destructor
            mClients.erase(mClients.begin() + i);
            return;
        }
    }
}

bool NativeHttpServer::isSocketConnected(const asio::ip::tcp::socket& sock) const FL_NO_EXCEPT {
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
