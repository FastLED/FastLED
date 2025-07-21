# FastLED Networking Refactor Design

## 🎯 Objective

Eliminate multiple platform-specific ServerSocket implementations and consolidate them into a single, non-polymorphic ServerSocket class that uses a **normalized POSIX-style socket API**. Each platform provides low-level socket API wrappers that expose the same POSIX interface, allowing ServerSocket to use identical code across all platforms.

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

### Normalized POSIX-Style Socket API

Each platform provides a **normalized POSIX-style socket API** in the `fl` namespace. All platforms expose identical function signatures, allowing ServerSocket to use the same implementation code everywhere.

**Platform Socket API** (`socket_win.h`, `socket_posix.h`):

```cpp
// src/platforms/win/socket_win.h
// src/platforms/posix/socket_posix.h
// All platforms expose identical API in fl namespace

namespace fl {

// Core Socket Operations
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);

// Addressing
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Data Transfer
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

// Connection Teardown
int shutdown(int sockfd, int how);
int close(int fd);

// Socket Options
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

// Peer & Local Address Retrieval
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Name-and-Service Translation
int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

// Supporting Types (platform-normalized)
typedef unsigned long socklen_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_in {           // IPv4
    sa_family_t sin_family;    // AF_INET
    in_port_t sin_port;        // htons(port)
    struct in_addr sin_addr;   // IPv4 address
    unsigned char sin_zero[8];
};

struct sockaddr_in6 {          // IPv6
    sa_family_t sin6_family;   // AF_INET6
    in_port_t sin6_port;       // htons(port)
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr; // IPv6 address
    uint32_t sin6_scope_id;
};

} // namespace fl
```

### Single ServerSocket Implementation

ServerSocket uses the normalized POSIX API directly - **no platform-specific implementations**:

```cpp
// src/fl/net/server_socket.h
class ServerSocket {
public:
    explicit ServerSocket(const SocketOptions& options = {});
    ~ServerSocket();

    // No virtual methods - direct implementation using fl:: socket API
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
    int mSocket = -1;  // Standard POSIX file descriptor
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

### ServerSocket Implementation Using Normalized API

```cpp
// src/fl/net/server_socket.cpp
#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/server_socket.h"

// Include platform-specific normalized API
#if defined(_WIN32)
    #include "platforms/win/socket_win.h"
#else
    #include "platforms/posix/socket_posix.h"
#endif

namespace fl {

ServerSocket::ServerSocket(const SocketOptions& options) : mOptions(options) {
    // Use normalized fl:: socket API - same code on all platforms
    mSocket = fl::socket(AF_INET, SOCK_STREAM, 0);
    if (mSocket == -1) {
        set_error(SocketError::SOCKET_ERROR, "Failed to create server socket");
        return;
    }
    
    setup_socket_options();
}

ServerSocket::~ServerSocket() {
    close();
}

SocketError ServerSocket::bind(const fl::string& address, int port) {
    if (mSocket == -1) {
        return SocketError::SOCKET_ERROR;
    }
    
    // Use normalized fl:: socket API - same code on all platforms
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    // Convert address string to binary form
    if (fl::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        set_error(SocketError::INVALID_ADDRESS, "Invalid address format");
        return SocketError::INVALID_ADDRESS;
    }
    
    // Same fl::bind() call on all platforms
    if (fl::bind(mSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        set_error(SocketError::BIND_ERROR, "Failed to bind server socket");
        return SocketError::BIND_ERROR;
    }
    
    mBoundAddress = address;
    mBoundPort = port;
    return SocketError::SUCCESS;
}

SocketError ServerSocket::listen(int backlog) {
    if (mSocket == -1) {
        return SocketError::SOCKET_ERROR;
    }
    
    // Same fl::listen() call on all platforms
    if (fl::listen(mSocket, backlog) == -1) {
        set_error(SocketError::LISTEN_ERROR, "Failed to listen on server socket");
        return SocketError::LISTEN_ERROR;
    }
    
    mIsListening = true;
    mBacklog = backlog;
    return SocketError::SUCCESS;
}

fl::shared_ptr<Socket> ServerSocket::accept() {
    if (!mIsListening || mSocket == -1) {
        return nullptr;
    }
    
    // Same fl::accept() call on all platforms
    sockaddr_in client_addr = {};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_socket = fl::accept(mSocket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_socket == -1) {
        return nullptr;
    }
    
    // Create client socket from the accepted connection
    auto client = SocketFactory::create_client_socket(mOptions);
    // TODO: Initialize client socket with the accepted socket handle
    
    mCurrentConnections++;
    return client;
}

void ServerSocket::close() {
    if (mSocket != -1) {
        // Same fl::close() call on all platforms
        fl::close(mSocket);
        mSocket = -1;
    }
    mIsListening = false;
    mCurrentConnections = 0;
}

void ServerSocket::set_reuse_address(bool enable) {
    if (mSocket != -1) {
        int optval = enable ? 1 : 0;
        // Same fl::setsockopt() call on all platforms
        fl::setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    }
}

void ServerSocket::set_non_blocking(bool non_blocking) {
    if (mSocket != -1) {
        // Platform-normalized non-blocking setup via fl:: API
        int flags = fl::fcntl(mSocket, F_GETFL, 0);
        if (non_blocking) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }
        fl::fcntl(mSocket, F_SETFL, flags);
        mIsNonBlocking = non_blocking;
    }
}

// ... other method implementations using normalized fl:: socket API

} // namespace fl

#endif // FASTLED_HAS_NETWORKING
```

### Platform Implementation Examples

**Windows Platform** (`src/platforms/win/socket_win.h`):
```cpp
// Windows-specific implementation that exposes POSIX-style API
namespace fl {

// Normalize Windows sockets to POSIX-style API
int socket(int domain, int type, int protocol) {
    SOCKET sock = ::socket(domain, type, protocol);
    return (sock == INVALID_SOCKET) ? -1 : static_cast<int>(sock);
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    SOCKET sock = static_cast<SOCKET>(sockfd);
    int result = ::bind(sock, addr, addrlen);
    return (result == SOCKET_ERROR) ? -1 : 0;
}

int listen(int sockfd, int backlog) {
    SOCKET sock = static_cast<SOCKET>(sockfd);
    int result = ::listen(sock, backlog);
    return (result == SOCKET_ERROR) ? -1 : 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    SOCKET server_sock = static_cast<SOCKET>(sockfd);
    SOCKET client_sock = ::accept(server_sock, addr, addrlen);
    return (client_sock == INVALID_SOCKET) ? -1 : static_cast<int>(client_sock);
}

int close(int fd) {
    SOCKET sock = static_cast<SOCKET>(fd);
    int result = ::closesocket(sock);
    return (result == SOCKET_ERROR) ? -1 : 0;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    SOCKET sock = static_cast<SOCKET>(sockfd);
    int result = ::send(sock, static_cast<const char*>(buf), static_cast<int>(len), flags);
    return (result == SOCKET_ERROR) ? -1 : static_cast<ssize_t>(result);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    SOCKET sock = static_cast<SOCKET>(sockfd);
    int result = ::recv(sock, static_cast<char*>(buf), static_cast<int>(len), flags);
    return (result == SOCKET_ERROR) ? -1 : static_cast<ssize_t>(result);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    SOCKET sock = static_cast<SOCKET>(sockfd);
    int result = ::setsockopt(sock, level, optname, static_cast<const char*>(optval), optlen);
    return (result == SOCKET_ERROR) ? -1 : 0;
}

// ... all other POSIX socket functions normalized for Windows

} // namespace fl
```

**POSIX Platform** (`src/platforms/posix/socket_posix.h`):
```cpp
// POSIX implementation - direct passthrough to system calls
namespace fl {

// Direct passthrough to POSIX socket API
inline int socket(int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
}

inline int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return ::bind(sockfd, addr, addrlen);
}

inline int listen(int sockfd, int backlog) {
    return ::listen(sockfd, backlog);
}

inline int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::accept(sockfd, addr, addrlen);
}

inline int close(int fd) {
    return ::close(fd);
}

inline ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return ::send(sockfd, buf, len, flags);
}

inline ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return ::recv(sockfd, buf, len, flags);
}

inline int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return ::setsockopt(sockfd, level, optname, optval, optlen);
}

// ... all other POSIX socket functions (direct passthrough)

} // namespace fl
```

## 🗂️ File Organization

### Files to Create

```
src/fl/net/
├── server_socket.h           # Single non-polymorphic ServerSocket class
├── server_socket.cpp         # ServerSocket implementation using fl:: socket API
└── socket_factory.h          # Updated to remove ServerSocket factory methods
```

### Files to Create (Platform Socket API)

```
src/platforms/win/
└── socket_win.h              # Windows POSIX-style socket API wrappers

src/platforms/posix/
└── socket_posix.h            # POSIX socket API passthrough (MOVE from shared/networking/)
```

**Platform Reorganization**: POSIX files are currently mislocated in `src/platforms/shared/networking/` but should be moved to `src/platforms/posix/` to match the pattern of other platforms. POSIX is a specific platform, not a shared implementation.

**🎯 CRITICAL ARCHITECTURE:**
- **Platform files** = **ONLY normalized POSIX-style socket API** (`fl::socket()`, `fl::bind()`, `fl::listen()`, etc.)
- **ServerSocket class** = **ONLY high-level server logic** using the normalized `fl::` socket API
- **NO platform-specific ServerSocket implementations** - single ServerSocket works on all platforms
- **NO stub implementations** - real ServerSocket uses real platform socket APIs

### Files to Remove

```
src/platforms/shared/networking/posix_socket.h    # Remove PosixServerSocket class
src/platforms/win/socket.h                        # Remove WinServerSocket class  
src/platforms/stub/net/socket.h                   # Remove ENTIRE StubServerSocket class
src/platforms/stub/net/socket.cpp                 # Remove StubServerSocket implementation
tests/test_networking_consolidated.cpp            # Remove TestServerSocket - use real implementations
```

**Critical**: We are eliminating ALL platform-specific ServerSocket implementations. The new design uses a single ServerSocket class that works identically on all platforms via the normalized socket API.

### Files to Update

```
src/fl/net/socket.h                               # Remove abstract ServerSocket base class
src/fl/net/socket_factory.h/.cpp                 # Remove ServerSocket factory methods
tests/test_networking_consolidated.cpp            # Update tests for new architecture
```

## 🚀 Migration Strategy

### Phase 1: Create Normalized Socket API

**🔧 IMPLEMENTATION TASKS**:
1. **Create platform socket API headers**:
   - `src/platforms/win/socket_win.h` - Windows-to-POSIX socket API normalization
   - `src/platforms/posix/socket_posix.h` - POSIX socket API passthrough (moved from shared/networking/)
2. **Implement all POSIX socket functions** in `fl` namespace on each platform
3. **Ensure identical function signatures** across all platforms
4. **Test normalized API** functions individually

### Phase 2: Implement Single ServerSocket

1. **Create non-polymorphic ServerSocket** class using `fl::` socket API
2. **Implement server logic** using normalized socket calls
3. **Update SocketFactory** to remove ServerSocket creation methods
4. **Test ServerSocket** with real platform socket implementations

### Phase 3: Remove Platform-Specific Implementations

1. **Remove abstract ServerSocket** base class from `socket.h`
2. **Remove ALL platform-specific** ServerSocket classes (Win, POSIX, Stub)
3. **Update all references** to use new ServerSocket class
4. **Remove ServerSocket factory methods**

### Phase 4: Update Tests

1. **Remove TestServerSocket** and stub implementations
2. **Migrate tests** to use real ServerSocket with real platform APIs
3. **Add tests for normalized socket API** functions
4. **Validate cross-platform** behavior consistency

## ✅ Benefits

### Architecture Benefits

- **Single Implementation**: One ServerSocket class instead of multiple platform-specific implementations
- **Normalized API**: All platforms expose identical POSIX-style socket interface
- **Zero Virtual Dispatch**: Direct function calls to platform socket wrappers
- **Platform Isolation**: Socket platform differences isolated to normalization layer

### Performance Benefits

- **No Virtual Dispatch**: Direct function calls instead of virtual method lookups
- **Better Inlining**: Compiler can inline platform socket wrappers
- **Reduced Memory**: Single ServerSocket class instead of multiple implementations
- **Faster Builds**: Less template instantiation and virtual function overhead

### Maintenance Benefits

- **Single Source of Truth**: One ServerSocket implementation to maintain
- **Consistent Behavior**: Platform differences isolated to socket API normalization
- **Easier Testing**: Platform socket functions can be tested independently
- **Simpler Factory**: No need for platform-specific ServerSocket factories
- **Clear Separation**: Business logic (ServerSocket) vs platform API (socket wrappers)

## 🔍 Compatibility Notes

### API Compatibility

- **Public API unchanged**: Existing ServerSocket users see no changes
- **Factory pattern simplified**: `SocketFactory::create_server_socket()` removed
- **Direct instantiation**: `ServerSocket socket(options)` replaces factory pattern

### Platform Compatibility

- **All platforms supported**: Windows, POSIX implementations provided
- **Feature parity maintained**: All current ServerSocket features preserved
- **POSIX normalization**: Windows socket API normalized to POSIX-style interface

### Testing Compatibility

- **No more fake ServerSocket classes**: TestServerSocket and StubServerSocket eliminated
- **Real platform implementations**: Tests use actual platform socket APIs
- **Real ServerSocket instances**: Tests use actual ServerSocket implementation
- **Platform API testing**: Normalized socket functions can be tested in isolation

**Critical Change**: We eliminate all fake/stub ServerSocket implementations because ServerSocket is no longer polymorphic. ServerSocket uses real platform socket APIs instead of platform-specific ServerSocket classes.

## 📋 Implementation Checklist

### ❌ What Needs Implementation  
- [ ] **Phase 1**: Create normalized socket API platform files:
  - [ ] Create `src/platforms/win/socket_win.h` with Windows-to-POSIX socket API normalization
  - [ ] Move and update `src/platforms/shared/networking/posix_socket.h` to `src/platforms/posix/socket_posix.h` with POSIX passthrough
  - [ ] Implement all POSIX socket functions (`fl::socket()`, `fl::bind()`, `fl::listen()`, `fl::accept()`, `fl::close()`, `fl::send()`, `fl::recv()`, `fl::setsockopt()`, etc.)
  - [ ] Ensure identical function signatures across platforms
- [ ] **Phase 2**: Create single ServerSocket implementation (`src/fl/net/server_socket.h/.cpp`) using `fl::` socket API
- [ ] **Phase 2**: Update SocketFactory to remove `create_server_socket()` method
- [ ] **Phase 3**: Remove abstract ServerSocket base class (currently in `src/fl/net/socket.h`)
- [ ] **Phase 3**: Remove ALL platform-specific ServerSocket implementations:
  - [ ] Remove `WinServerSocket` class 
  - [ ] Remove `PosixServerSocket` class
  - [ ] Remove `StubServerSocket` class
- [ ] **Phase 4**: Update test architecture:
  - [ ] Remove `TestServerSocket` class
  - [ ] Remove platform ServerSocket factory functions
  - [ ] Add tests using real ServerSocket with real platform socket APIs
- [ ] **Phase 4**: Add normalized socket API unit tests
- [ ] **Phase 4**: Validate cross-platform compatibility

### 📊 Current Reality Check
**None of the refactor has been implemented yet.** The codebase still has:
- Abstract `ServerSocket` base class with virtual methods
- Platform-specific `WinServerSocket`, `PosixServerSocket`, `StubServerSocket` classes
- Factory pattern creating ServerSocket instances
- Test fakes using `TestServerSocket` inheritance

---

**🎯 Success Criteria**: Single ServerSocket class using normalized `fl::` socket API, with platform files providing POSIX-style socket wrappers, maintaining full API compatibility while improving performance and maintainability. 
