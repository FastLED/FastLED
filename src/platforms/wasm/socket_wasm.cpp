#ifdef FASTLED_HAS_NETWORKING
// Only compile WASM socket implementation for WASM targets
#if defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(__wasm32__)

#include "socket_wasm.h"
#include "fl/str.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>  // For snprintf

// Add missing network byte order functions for pure WASM environments only
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
    // For pure WASM (not Emscripten, not Windows)
    static inline unsigned short htons(unsigned short x) { return (x >> 8) | (x << 8); }
    static inline unsigned short ntohs(unsigned short x) { return htons(x); }
    static inline unsigned long htonl(unsigned long x) { return (x >> 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | (x << 24); }
    static inline unsigned long ntohl(unsigned long x) { return htonl(x); }
    
    #define EAI_BADFLAGS    -1
    #define EAI_NONAME      -2
    #define EAI_MEMORY      -10
    #define EAI_FAIL        -4
    #define EAI_FAMILY      -6
    #define ENOPROTOOPT     109
    #define EBADF           9
    #define EFAULT          14
#endif

namespace fl {

//=============================================================================
// WASM Socket State Management (Fake Implementation)
//=============================================================================

// Global state for WASM socket simulation
static int g_next_socket_fd = 1000;  // Start with high numbers to avoid conflicts
static bool g_initialized = false;
static bool g_mock_should_fail = false;
static int g_mock_error_code = ECONNREFUSED;

// Statistics tracking
static WasmSocketStats g_stats = {};

// Simple socket registry to track "open" sockets
static bool g_socket_registry[256] = {};  // Track up to 256 fake sockets

//=============================================================================
// Helper Functions for WASM Socket Simulation
//=============================================================================

static int allocate_socket_fd() {
    g_stats.total_sockets_created++;
    
    // Find next available slot in registry
    for (int i = 0; i < 256; i++) {
        int fd = 1000 + i;
        if (!g_socket_registry[i]) {
            g_socket_registry[i] = true;
            return fd;
        }
    }
    
    // If no slots available, just increment counter
    return g_next_socket_fd++;
}

static bool is_valid_socket_fd(int sockfd) {
    if (sockfd < 1000 || sockfd >= 1256) {
        return false;
    }
    return g_socket_registry[sockfd - 1000];
}

static void deallocate_socket_fd(int sockfd) {
    if (sockfd >= 1000 && sockfd < 1256) {
        g_socket_registry[sockfd - 1000] = false;
    }
}

static void set_mock_errno(int error_code) {
    // WASM doesn't have real errno, so we just track it internally
    g_mock_error_code = error_code;
}

static void fill_mock_sockaddr_in(struct sockaddr_in* addr, const char* ip, int port) {
    if (!addr) return;
    
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    
    // Simple IP parsing for common cases
    if (strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "localhost") == 0) {
        addr->sin_addr.s_addr = htonl(0x7F000001);  // 127.0.0.1
    } else if (strcmp(ip, "fastled.io") == 0) {
        addr->sin_addr.s_addr = htonl(0x5DB8D822);  // Fake IP for fastled.io
    } else {
        addr->sin_addr.s_addr = htonl(0xC0A80001);  // 192.168.0.1 default
    }
    
    memset(addr->sin_zero, 0, 8);
}

//=============================================================================
// Core Socket Operations (Fake Implementation)
//=============================================================================

int socket(int domain, int type, int protocol) {
    if (!g_initialized) {
        initialize_wasm_sockets();
    }
    
    // Validate parameters
    if (domain != AF_INET && domain != AF_INET6) {
        set_mock_errno(EAFNOSUPPORT);
        return -1;
    }
    
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        set_mock_errno(EINVAL);
        return -1;
    }
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    return allocate_socket_fd();
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    // WASM doesn't support socketpair - return error
    (void)domain; (void)type; (void)protocol; (void)sv;
    set_mock_errno(EAFNOSUPPORT);
    return -1;
}

//=============================================================================
// Addressing Operations (Fake Implementation)
//=============================================================================

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!addr || addrlen < sizeof(struct sockaddr)) {
        set_mock_errno(EINVAL);
        return -1;
    }
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    // Simulate successful bind
    return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!addr || addrlen < sizeof(struct sockaddr)) {
        set_mock_errno(EINVAL);
        return -1;
    }
    
    g_stats.total_connections_attempted++;
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    // Simulate successful connection
    return 0;
}

int listen(int sockfd, int backlog) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (backlog < 0) {
        set_mock_errno(EINVAL);
        return -1;
    }
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    // Simulate successful listen
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    // Simulate accepted connection
    int client_fd = allocate_socket_fd();
    
    // Fill in mock client address if requested
    if (addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        fill_mock_sockaddr_in((struct sockaddr_in*)addr, "127.0.0.1", 12345);
        *addrlen = sizeof(struct sockaddr_in);
    }
    
    return client_fd;
}

//=============================================================================
// Data Transfer Operations (Fake Implementation)
//=============================================================================

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!buf && len > 0) {
        set_mock_errno(EFAULT);
        return -1;
    }
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    // Simulate successful send of all data
    g_stats.total_bytes_sent += len;
    return static_cast<ssize_t>(len);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!buf && len > 0) {
        set_mock_errno(EFAULT);
        return -1;
    }
    
    if (g_mock_should_fail) {
        set_mock_errno(g_mock_error_code);
        return -1;
    }
    
    // Simulate receiving mock data
    const char* mock_response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nServer: WASM-Mock\r\n\r\nMock response from WASM socket";
    size_t response_len = strlen(mock_response);
    size_t copy_len = (len < response_len) ? len : response_len;
    
    memcpy(buf, mock_response, copy_len);
    g_stats.total_bytes_received += copy_len;
    
    return static_cast<ssize_t>(copy_len);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    // For UDP sockets - just delegate to send for simplicity
    (void)dest_addr; (void)addrlen;
    return send(sockfd, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
    // For UDP sockets - delegate to recv and fill mock source address
    ssize_t result = recv(sockfd, buf, len, flags);
    
    if (result > 0 && src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        fill_mock_sockaddr_in((struct sockaddr_in*)src_addr, "192.168.1.100", 54321);
        *addrlen = sizeof(struct sockaddr_in);
    }
    
    return result;
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    // Simplified implementation - just count total bytes in iovec
    if (!is_valid_socket_fd(sockfd) || !msg) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    size_t total_len = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        total_len += msg->msg_iov[i].iov_len;
    }
    
    return send(sockfd, nullptr, total_len, flags);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    // Simplified implementation
    if (!is_valid_socket_fd(sockfd) || !msg || msg->msg_iovlen == 0) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    // Just fill first iovec with mock data
    return recv(sockfd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, flags);
}

//=============================================================================
// Connection Teardown (Fake Implementation)
//=============================================================================

int shutdown(int sockfd, int how) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (how < SHUT_RD || how > SHUT_RDWR) {
        set_mock_errno(EINVAL);
        return -1;
    }
    
    // Simulate successful shutdown
    return 0;
}

int close(int fd) {
    if (fd >= 1000 && fd < 1256) {
        deallocate_socket_fd(fd);
        return 0;
    }
    
    // For non-socket file descriptors, just return success
    return 0;
}

//=============================================================================
// Socket Options (Fake Implementation)
//=============================================================================

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!optval && optlen > 0) {
        set_mock_errno(EFAULT);
        return -1;
    }
    
    // Simulate successful socket option setting
    return 0;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!optval || !optlen) {
        set_mock_errno(EFAULT);
        return -1;
    }
    
    // Return mock values for common socket options
    if (level == SOL_SOCKET && optname == SO_REUSEADDR && *optlen >= sizeof(int)) {
        *((int*)optval) = 1;
        *optlen = sizeof(int);
        return 0;
    }
    
    set_mock_errno(ENOPROTOOPT);
    return -1;
}

//=============================================================================
// Address Retrieval (Fake Implementation)
//=============================================================================

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!addr || !addrlen || *addrlen < sizeof(struct sockaddr_in)) {
        set_mock_errno(EFAULT);
        return -1;
    }
    
    // Fill mock peer address
    fill_mock_sockaddr_in((struct sockaddr_in*)addr, "93.184.216.34", 80);  // example.com IP
    *addrlen = sizeof(struct sockaddr_in);
    
    return 0;
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (!is_valid_socket_fd(sockfd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    if (!addr || !addrlen || *addrlen < sizeof(struct sockaddr_in)) {
        set_mock_errno(EFAULT);
        return -1;
    }
    
    // Fill mock local address
    fill_mock_sockaddr_in((struct sockaddr_in*)addr, "127.0.0.1", 12345);
    *addrlen = sizeof(struct sockaddr_in);
    
    return 0;
}

//=============================================================================
// Name Resolution (Fake Implementation)
//=============================================================================

int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res) {
    if (!res) {
        return EAI_BADFLAGS;
    }
    
    *res = nullptr;
    
    if (!node && !service) {
        return EAI_NONAME;
    }
    
    // Allocate mock addrinfo structure
    struct addrinfo *ai = (struct addrinfo*)malloc(sizeof(struct addrinfo));
    struct sockaddr_in *sin = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    
    if (!ai || !sin) {
        free(ai);
        free(sin);
        return EAI_MEMORY;
    }
    
    // Fill mock addrinfo
    ai->ai_flags = 0;
    ai->ai_family = AF_INET;
    ai->ai_socktype = SOCK_STREAM;
    ai->ai_protocol = IPPROTO_TCP;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_addr = (struct sockaddr*)sin;
    ai->ai_canonname = nullptr;
    ai->ai_next = nullptr;
    
    // Fill mock address
    const char* ip = "127.0.0.1";
    if (node) {
        if (strcmp(node, "fastled.io") == 0) {
            ip = "93.184.216.34";  // Mock IP for fastled.io
        } else if (strcmp(node, "localhost") == 0) {
            ip = "127.0.0.1";
        }
    }
    
    int port = 80;
    if (service) {
        if (strcmp(service, "http") == 0) port = 80;
        else if (strcmp(service, "https") == 0) port = 443;
        else port = atoi(service);
    }
    
    fill_mock_sockaddr_in(sin, ip, port);
    
    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    while (res) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res->ai_canonname);
        free(res);
        res = next;
    }
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    if (!sa || salen < sizeof(struct sockaddr)) {
        return EAI_FAIL;
    }
    
    if (sa->sa_family == AF_INET && salen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *sin = (const struct sockaddr_in*)sa;
        
        if (host && hostlen > 0) {
            strncpy(host, "mock.wasm.host", hostlen - 1);
            host[hostlen - 1] = '\0';
        }
        
        if (serv && servlen > 0) {
            snprintf(serv, servlen, "%d", ntohs(sin->sin_port));
        }
        
        return 0;
    }
    
    return EAI_FAMILY;
}

//=============================================================================
// Address Conversion (Fake Implementation)
//=============================================================================

int inet_pton(int af, const char *src, void *dst) {
    if (!src || !dst) {
        return 0;
    }
    
    if (af == AF_INET) {
        // Simple IP parsing for testing
        unsigned long addr = 0;
        if (strcmp(src, "127.0.0.1") == 0) {
            addr = htonl(0x7F000001);
        } else if (strcmp(src, "192.168.1.1") == 0) {
            addr = htonl(0xC0A80101);
        } else if (strcmp(src, "93.184.216.34") == 0) {
            addr = htonl(0x5DB8D822);
        } else {
            return 0;  // Invalid address
        }
        
        *((unsigned long*)dst) = addr;
        return 1;
    }
    
    return 0;  // Unsupported address family
}

const char* inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (!src || !dst || size == 0) {
        return nullptr;
    }
    
    if (af == AF_INET && size >= 16) {  // INET_ADDRSTRLEN
        unsigned long addr = *((const unsigned long*)src);
        addr = ntohl(addr);
        
        snprintf(dst, size, "%lu.%lu.%lu.%lu",
                (addr >> 24) & 0xFF,
                (addr >> 16) & 0xFF,
                (addr >> 8) & 0xFF,
                addr & 0xFF);
        
        return dst;
    }
    
    return nullptr;
}

//=============================================================================
// File Control (Fake Implementation)
//=============================================================================

int fcntl(int fd, int cmd, ...) {
    if (!is_valid_socket_fd(fd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    // Handle common fcntl operations
    switch (cmd) {
        case F_GETFL:
            return 0;  // Return no flags set
            
        case F_SETFL:
            return 0;  // Simulate successful flag setting
            
        default:
            set_mock_errno(EINVAL);
            return -1;
    }
}

//=============================================================================
// I/O Control (Fake Implementation)
//=============================================================================

int ioctl(int fd, unsigned long request, ...) {
    if (!is_valid_socket_fd(fd)) {
        set_mock_errno(EBADF);
        return -1;
    }
    
    // Simulate successful ioctl
    return 0;
}

//=============================================================================
// Error Handling (Fake Implementation)
//=============================================================================

int get_errno() {
    return g_mock_error_code;
}

//=============================================================================
// WASM-specific Helper Functions
//=============================================================================

bool initialize_wasm_sockets() {
    if (g_initialized) {
        return true;
    }
    
    // Initialize state
    memset(g_socket_registry, 0, sizeof(g_socket_registry));
    memset(&g_stats, 0, sizeof(g_stats));
    
    g_initialized = true;
    return true;
}

void cleanup_wasm_sockets() {
    if (!g_initialized) {
        return;
    }
    
    // Close all open sockets
    for (int i = 0; i < 256; i++) {
        if (g_socket_registry[i]) {
            g_socket_registry[i] = false;
        }
    }
    
    g_initialized = false;
}

void set_wasm_socket_mock_behavior(bool should_fail, int error_code) {
    g_mock_should_fail = should_fail;
    g_mock_error_code = error_code;
    g_stats.mock_mode_enabled = should_fail;
    g_stats.mock_error_code = error_code;
}

WasmSocketStats get_wasm_socket_stats() {
    return g_stats;
}

void reset_wasm_socket_stats() {
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.mock_mode_enabled = g_mock_should_fail;
    g_stats.mock_error_code = g_mock_error_code;
}

} // namespace fl

#endif // WASM platform
#endif // FASTLED_HAS_NETWORKING 
