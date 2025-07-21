# FastLED Networking Refactor Design

## 🎯 Objective

Eliminate multiple platform-specific ServerSocket implementations and consolidate them into a single, non-polymorphic ServerSocket class. The polymorphic code should remain in the platform_socket layer for low-level socket operations.

## 🚨 Current Architecture Problems

### Multiple ServerSocket Implementations

Currently, we have multiple platform-specific ServerSocket implementations:

- **PosixServerSocket** (`src/platforms/shared/networking/posix_socket.h`)
- **WinServerSocket** (`src/platforms/win/socket.h`) 
- **StubServerSocket** (for testing)

Each inherits from the abstract `ServerSocket` base class:

```cpp
class ServerSocket {
public:
    virtual ~ServerSocket() = default;
    virtual SocketError bind(const fl::string& address, int port) = 0;
    virtual SocketError listen(int backlog = 5) = 0;
    virtual fl::shared_ptr<Socket> accept() = 0;
    // ... more virtual methods
};
```

### Problems with Current Approach

1. **Code Duplication**: Each platform reimplements nearly identical server logic
2. **Virtual Call Overhead**: Virtual dispatch for every server operation
3. **Maintenance Burden**: Changes require updates across multiple platform implementations
4. **Complex Factory Pattern**: Platform-specific factory functions for server socket creation
5. **Inconsistent Behavior**: Subtle differences between platform implementations

## ✅ Proposed Solution

### Single ServerSocket Class

Replace the polymorphic ServerSocket hierarchy with a single, non-polymorphic class that delegates to platform-specific functions:

```cpp
// src/fl/net/server_socket.h
class ServerSocket {
public:
    explicit ServerSocket(const SocketOptions& options = {});
    ~ServerSocket();

    // No virtual methods - direct implementation
    SocketError bind(const fl::string& address, int port);
    SocketError listen(int backlog = 5);
    void close();
    bool is_listening() const;
    
    fl::shared_ptr<Socket> accept();
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10);
    bool has_pending_connections() const;
    
    void set_reuse_address(bool enable);
    void set_reuse_port(bool enable);
    void set_non_blocking(bool non_blocking);
    
    fl::string bound_address() const;
    int bound_port() const;
    fl::size max_connections() const;
    fl::size current_connections() const;
    
    SocketError get_last_error() const;
    fl::string get_error_message() const;
    int get_socket_handle() const;

private:
    // Platform-neutral member variables
    SocketOptions mOptions;
    socket_handle_t mSocket = INVALID_SOCKET_HANDLE;
    bool mIsListening = false;
    fl::string mBoundAddress;
    int mBoundPort = 0;
    int mBacklog = 5;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    bool mIsNonBlocking = false;
    fl::size mCurrentConnections = 0;
    
    // Internal helpers
    void set_error(SocketError error, const fl::string& message = "");
    bool setup_socket_options();
};
```

### Platform Socket Functions

The platform-specific logic moves to platform_* functions (similar to existing socket.hpp pattern):

#### Platform Function Interface

```cpp
// Platform-specific server socket functions
// Each platform implements these directly

namespace fl {

// Server socket lifecycle
socket_handle_t platform_create_server_socket();
SocketError platform_bind_server_socket(socket_handle_t socket, const fl::string& address, int port);
SocketError platform_listen_server_socket(socket_handle_t socket, int backlog);
socket_handle_t platform_accept_connection(socket_handle_t server_socket);
void platform_close_server_socket(socket_handle_t socket);

// Server socket configuration
bool platform_set_server_socket_reuse_address(socket_handle_t socket, bool enable);
bool platform_set_server_socket_reuse_port(socket_handle_t socket, bool enable);
bool platform_set_server_socket_non_blocking(socket_handle_t socket, bool non_blocking);

// Server socket status queries
bool platform_server_socket_has_pending_connections(socket_handle_t socket);
fl::size platform_get_server_socket_pending_count(socket_handle_t socket);

// Socket information
fl::string platform_get_server_socket_bound_address(socket_handle_t socket);
int platform_get_server_socket_bound_port(socket_handle_t socket);

} // namespace fl
```

#### Platform Implementation Examples

**Windows Platform** (`src/platforms/win/server_socket.hpp`):
```cpp
namespace fl {

socket_handle_t platform_create_server_socket() {
    return from_platform_socket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
}

SocketError platform_bind_server_socket(socket_handle_t handle, const fl::string& address, int port) {
    socket_t sock = to_platform_socket(handle);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (!platform_inet_pton(address.c_str(), &addr.sin_addr)) {
        return SocketError::INVALID_ADDRESS;
    }
    
    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return platform_translate_socket_error(WSAGetLastError());
    }
    
    return SocketError::SUCCESS;
}

SocketError platform_listen_server_socket(socket_handle_t handle, int backlog) {
    socket_t sock = to_platform_socket(handle);
    if (::listen(sock, backlog) == SOCKET_ERROR) {
        return platform_translate_socket_error(WSAGetLastError());
    }
    return SocketError::SUCCESS;
}

socket_handle_t platform_accept_connection(socket_handle_t server_handle) {
    socket_t server_sock = to_platform_socket(server_handle);
    sockaddr_in client_addr = {};
    int addr_len = sizeof(client_addr);
    
    socket_t client_sock = ::accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    return from_platform_socket(client_sock);
}

// ... other platform implementations

} // namespace fl
```

**POSIX Platform** (`src/platforms/posix/posix_server_socket.hpp`):
```cpp
namespace fl {

socket_handle_t platform_create_server_socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

SocketError platform_bind_server_socket(socket_handle_t socket, const fl::string& address, int port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return SocketError::INVALID_ADDRESS;
    }
    
    if (::bind(socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        return posix_translate_socket_error(errno);
    }
    
    return SocketError::SUCCESS;
}

// ... other platform implementations

} // namespace fl
```

### ServerSocket Implementation

The unified ServerSocket class delegates to platform functions:

```cpp
// src/fl/net/server_socket.cpp
#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/server_socket.h"

// Include platform-specific headers
#if defined(_WIN32)
    #include "platforms/win/server_socket.hpp"
#else
    #include "platforms/posix/posix_server_socket.hpp"
#endif

// Note: No stub implementation - tests use the real ServerSocket with real platform implementations

namespace fl {

ServerSocket::ServerSocket(const SocketOptions& options) : mOptions(options) {
    mSocket = platform_create_server_socket();
    if (mSocket == INVALID_SOCKET_HANDLE) {
        set_error(SocketError::SOCKET_ERROR, "Failed to create server socket");
        return;
    }
    
    setup_socket_options();
}

ServerSocket::~ServerSocket() {
    close();
}

SocketError ServerSocket::bind(const fl::string& address, int port) {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return SocketError::SOCKET_ERROR;
    }
    
    SocketError result = platform_bind_server_socket(mSocket, address, port);
    if (result == SocketError::SUCCESS) {
        mBoundAddress = address;
        mBoundPort = port;
    } else {
        set_error(result, "Failed to bind server socket");
    }
    
    return result;
}

SocketError ServerSocket::listen(int backlog) {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return SocketError::SOCKET_ERROR;
    }
    
    SocketError result = platform_listen_server_socket(mSocket, backlog);
    if (result == SocketError::SUCCESS) {
        mIsListening = true;
        mBacklog = backlog;
    } else {
        set_error(result, "Failed to listen on server socket");
    }
    
    return result;
}

fl::shared_ptr<Socket> ServerSocket::accept() {
    if (!mIsListening || mSocket == INVALID_SOCKET_HANDLE) {
        return nullptr;
    }
    
    socket_handle_t client_socket = platform_accept_connection(mSocket);
    if (client_socket == INVALID_SOCKET_HANDLE) {
        return nullptr;
    }
    
    // Create client socket from the accepted connection
    // This uses the existing Socket factory pattern
    auto client = SocketFactory::create_client_socket(mOptions);
    // TODO: Initialize client socket with the accepted socket handle
    
    mCurrentConnections++;
    return client;
}

void ServerSocket::close() {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_close_server_socket(mSocket);
        mSocket = INVALID_SOCKET_HANDLE;
    }
    mIsListening = false;
    mCurrentConnections = 0;
}

bool ServerSocket::has_pending_connections() const {
    if (!mIsListening || mSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }
    return platform_server_socket_has_pending_connections(mSocket);
}

void ServerSocket::set_reuse_address(bool enable) {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_set_server_socket_reuse_address(mSocket, enable);
    }
}

void ServerSocket::set_reuse_port(bool enable) {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_set_server_socket_reuse_port(mSocket, enable);
    }
}

void ServerSocket::set_non_blocking(bool non_blocking) {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_set_server_socket_non_blocking(mSocket, non_blocking);
        mIsNonBlocking = non_blocking;
    }
}

// ... other method implementations

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
```

## 🗂️ File Organization

### Files to Create

```
src/fl/net/
├── server_socket.h           # Non-polymorphic ServerSocket class
├── server_socket.cpp         # ServerSocket implementation
└── socket_factory.h          # Updated to remove ServerSocket factory methods
```

### Files to Extend (Add server functions to existing platform files)

```
src/platforms/win/
└── socket.hpp               # ADD server platform functions to existing file

src/platforms/posix/
└── posix_socket.h           # ADD server platform functions (MOVE from shared/networking/)
```

**Platform Reorganization Needed**: POSIX files are currently mislocated in `src/platforms/shared/networking/` but should be moved to `src/platforms/posix/` to match the pattern of other platforms (win/, stub/, esp/, etc.). POSIX is a specific platform, not a shared implementation.

**Note**: We deliberately DO NOT create separate server socket files. Server platform functions are added to the existing platform files alongside the current client socket functions.

### Files to Remove

```
src/platforms/shared/networking/posix_socket.h    # Remove PosixServerSocket class
src/platforms/win/socket.h                        # Remove WinServerSocket class
src/platforms/stub/net/socket.h                   # Remove ENTIRE StubServerSocket class - NO stub implementations
src/platforms/stub/net/socket.cpp                 # Remove StubServerSocket implementation
tests/test_networking_consolidated.cpp            # Remove TestServerSocket - use real implementations instead
```

**Critical**: We are eliminating ALL stub/fake ServerSocket implementations. The new design uses real ServerSocket instances with real platform implementations, not fake ServerSocket classes.

### Files to Update

```
src/fl/net/socket.h                               # Remove abstract ServerSocket base class
src/fl/net/socket_factory.h/.cpp                 # Remove ServerSocket factory methods
tests/test_networking_consolidated.cpp            # Update tests for new architecture
```

## 🚀 Migration Strategy

### Phase 1: Extend Platform Functions

**✅ ALREADY COMPLETED**: The platform function interface pattern is already established and working:
- `platform_create_socket()`, `platform_connect_socket()`, `platform_send_data()`, etc.
- Pattern exists in `src/platforms/win/socket.hpp`, should be in `src/platforms/posix/posix_socket.h`
- Consistent across Windows, POSIX, and other platforms

**🔧 REMAINING WORK**:
1. **Extend existing pattern** to include server-specific operations:
   - `platform_create_server_socket()`, `platform_bind_server_socket()`, `platform_listen_server_socket()`, `platform_accept_connection()`
2. **Add server functions** to existing `.hpp` files (don't create new files)
3. **Test new server functions** individually with unit tests

### Phase 2: Implement Unified ServerSocket

1. **Create non-polymorphic ServerSocket** class
2. **Implement delegation** to platform functions
3. **Update SocketFactory** to remove ServerSocket creation methods

### Phase 3: Remove Old Implementations

1. **Remove abstract ServerSocket** base class from `socket.h`
2. **Remove platform-specific** ServerSocket classes
3. **Update all references** to use new ServerSocket class

### Phase 4: Update Tests

1. **Migrate existing tests** to use new ServerSocket API
2. **Add tests for platform functions** directly
3. **Validate cross-platform** behavior consistency

## ✅ Benefits

### Performance Improvements

- **No Virtual Dispatch**: Direct function calls instead of virtual method lookups
- **Better Inlining**: Compiler can inline platform functions across translation units
- **Reduced Memory**: Single ServerSocket class instead of multiple implementations

### Maintenance Improvements

- **Single Source of Truth**: One ServerSocket implementation to maintain
- **Consistent Behavior**: Platform differences isolated to platform_* functions
- **Easier Testing**: Platform functions can be tested independently
- **Simpler Factory**: No need for platform-specific ServerSocket factories

### Code Quality Improvements

- **Reduced Duplication**: Common server logic implemented once
- **Clear Separation**: Business logic (ServerSocket) vs platform details (platform_*)
- **Better Documentation**: Single class to document instead of multiple implementations
- **Easier Debugging**: Single call stack path for server operations

## 🔍 Compatibility Notes

### API Compatibility

- **Public API unchanged**: Existing ServerSocket users see no changes
- **Factory pattern simplified**: `SocketFactory::create_server_socket()` removed
- **Direct instantiation**: `ServerSocket socket(options)` replaces factory pattern

### Platform Compatibility

- **All platforms supported**: Windows, POSIX, Stub implementations provided
- **Feature parity maintained**: All current ServerSocket features preserved
- **Platform-specific features**: Handled through platform_* function variations

### Testing Compatibility

- **No more fake ServerSocket classes**: TestServerSocket and StubServerSocket eliminated
- **Real platform implementations**: Tests use actual platform implementations instead of fake classes
- **Real ServerSocket instances**: Tests use actual ServerSocket implementation with real platform layer
- **Improved testability**: Platform functions can be tested in isolation
- **Real implementation testing**: Tests use actual platform implementations, no fakes

**Critical Change**: We eliminate all fake/stub ServerSocket implementations (TestServerSocket, StubServerSocket) because ServerSocket is no longer polymorphic. Tests should use real platform implementations instead of creating fake ServerSocket subclasses.

## 📋 Implementation Checklist

### ✅ What's Already Completed
- [x] **Phase 1**: Platform function interface pattern (see `platform_create_socket()`, `platform_connect_socket()`, etc. in `src/platforms/win/socket.hpp`)

### ❌ What Still Needs Implementation  
- [ ] **Phase 1**: Reorganize POSIX platform files:
  - [ ] Move `src/platforms/shared/networking/posix_socket.h/.cpp` to `src/platforms/posix/posix_socket.h/.cpp`
  - [ ] Update include paths and platform selection logic in `src/platforms/socket_platform.h`
- [ ] **Phase 1**: Add server socket platform functions to existing platform files:
  - [ ] Add `platform_create_server_socket()`, `platform_bind_server_socket()`, `platform_listen_server_socket()`, `platform_accept_connection()` to `src/platforms/win/socket.hpp`
  - [ ] Add same functions to `src/platforms/posix/posix_socket.h` (after reorganization)
  - [ ] Note: No stub platform functions needed per design
- [ ] **Phase 2**: Create unified ServerSocket class (`src/fl/net/server_socket.h/.cpp`) 
- [ ] **Phase 2**: Update SocketFactory to remove `create_server_socket()` method (currently in `src/fl/net/socket_factory.h/.cpp`)
- [ ] **Phase 3**: Remove abstract ServerSocket base class (currently in `src/fl/net/socket.h`)
- [ ] **Phase 3**: Remove platform-specific ServerSocket implementations:
  - [ ] Remove `WinServerSocket` class from `src/platforms/win/socket.h/.cpp`
  - [ ] Remove `PosixServerSocket` class from `src/platforms/posix/posix_socket.h/.cpp` (MOVE from shared/networking/) 
  - [ ] Remove `StubServerSocket` class from `src/platforms/stub/net/socket.h/.cpp`
- [ ] **Phase 4**: Update test architecture:
  - [ ] Remove `TestServerSocket` class from `tests/test_networking_consolidated.cpp`
  - [ ] Remove `create_platform_server_socket()` test factory function
  - [ ] Add tests that use real platform implementations instead of fake ServerSocket classes
- [ ] **Phase 4**: Add platform function unit tests
- [ ] **Phase 4**: Validate cross-platform compatibility

### 📊 Current Reality Check
**None of the refactor has been implemented yet.** The codebase still has:
- Abstract `ServerSocket` base class with virtual methods
- Platform-specific `WinServerSocket`, `PosixServerSocket`, `StubServerSocket` classes
- Factory pattern creating ServerSocket instances via `create_platform_server_socket()`
- Test fakes using `TestServerSocket` inheritance

---

**🎯 Success Criteria**: Single ServerSocket class with no virtual methods, delegating to platform_* functions for all platform-specific operations, maintaining full API compatibility while improving performance and maintainability. 
