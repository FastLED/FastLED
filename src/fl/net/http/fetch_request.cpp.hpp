#pragma once

#include "fl/net/http/fetch_request.h"
#include "fl/task/executor.h"
#include "fl/log/log.h"
#include "fl/stl/int.h"

#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/cstring.h"  // For memcpy

// Platform-specific socket includes
#ifdef FL_IS_WIN
    // Minimize Windows headers to avoid conflicts with Arduino types
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOGDI
        #define NOGDI  // Exclude GDI headers (prevents CRGB conflict)
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX  // Exclude min/max macros
    #endif

    // Undef Arduino macros that conflict with Windows SDK headers
    #ifdef INPUT
        #undef INPUT
    #endif
    #ifdef OUTPUT
        #undef OUTPUT
    #endif

    // Prevent Windows from typedef'ing types that conflict with Arduino
    #define boolean _FL_BOOLEAN_WORKAROUND
    #define CRGB _FL_CRGB_WORKAROUND

    // IWYU pragma: begin_keep
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // IWYU pragma: end_keep

    // Undefine workarounds
    #undef boolean
    #undef CRGB

    #define SOCKET_ERROR_WOULD_BLOCK WSAEWOULDBLOCK
    #define GET_SOCKET_ERROR() WSAGetLastError()
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    // IWYU pragma: begin_keep
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#include "fl/stl/noexcept.h"
    // IWYU pragma: end_keep
    #define SOCKET_ERROR_WOULD_BLOCK EWOULDBLOCK
    #define GET_SOCKET_ERROR() errno
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace fl {
namespace net {
namespace http {

namespace {

// The value socket() returns on failure: INVALID_SOCKET on Winsock, -1 on POSIX.
#ifdef FL_IS_WIN
constexpr fl::uptr kInvalidSocketHandle = static_cast<fl::uptr>(INVALID_SOCKET);
#else
constexpr int kInvalidSocketHandle = -1;
#endif

// ASCII case-insensitive equality; HTTP header names are case-insensitive.
bool header_name_equals(const fl::string& name, const char* reserved) FL_NO_EXCEPT {
    fl::size i = 0;
    for (; i < name.size() && reserved[i] != '\0'; ++i) {
        char a = name[i];
        char b = reserved[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) {
            return false;
        }
    }
    return i == name.size() && reserved[i] == '\0';
}

// CR or LF would end the current request line or header and let the rest of
// the field be read as a new one.
bool has_line_break(const fl::string& field) FL_NO_EXCEPT {
    for (fl::size i = 0; i < field.size(); ++i) {
        if (field[i] == '\r' || field[i] == '\n') {
            return true;
        }
    }
    return false;
}

// Headers FetchRequest writes itself: Host, Connection, and the body framing.
bool is_reserved_request_header(const fl::string& name) FL_NO_EXCEPT {
    return header_name_equals(name, "host") ||
           header_name_equals(name, "connection") ||
           header_name_equals(name, "content-length") ||
           header_name_equals(name, "transfer-encoding");
}

}  // namespace

namespace detail {

bool build_http_request(const RequestOptions& options, const fl::string& path,
                        const fl::string& host, fl::string* out) FL_NO_EXCEPT {
    if (has_line_break(options.method) || has_line_break(path) || has_line_break(host)) {
        return false;
    }
    for (const auto& header : options.headers) {
        if (has_line_break(header.first) || has_line_break(header.second)) {
            return false;
        }
    }
    fl::string request = options.method.empty() ? fl::string("GET") : options.method;
    request += " " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    for (const auto& header : options.headers) {
        if (is_reserved_request_header(header.first)) {
            continue;
        }
        request += header.first + ": " + header.second + "\r\n";
    }
    if (!options.body.empty()) {
        request += "Content-Length: ";
        request.append(static_cast<u32>(options.body.size()));
        request += "\r\n";
    }
    request += "Connection: close\r\n\r\n";
    request += options.body;
    *out = request;
    return true;
}

}  // namespace detail


FetchRequest::FetchRequest(const fl::string& url, const FetchOptions& opts, fl::task::Promise<Response> prom) FL_NO_EXCEPT
    : mState(DNS_LOOKUP)
    , mPromise(prom)
    , mOptions(opts.options())
    , mParsedUrl(url)
    , mHostname()
    , mPort(80)
    , mPath("/")
    , mSocketFd(kInvalidSocketHandle)
    , mDnsResult(nullptr)
    , mRequestBuffer()
    , mResponseBuffer()
    , mBytesSent(0)
    , mStateStartTime(fl::millis())
{
    if (!mParsedUrl.isValid() || mParsedUrl.host().empty()) {
        complete_error("Invalid URL");
        return;
    }
    mHostname = fl::string(mParsedUrl.host().data(), mParsedUrl.host().size());
    mPort = mParsedUrl.port();
    fl::string_view p = mParsedUrl.path();
    mPath = p.empty() ? fl::string("/") : fl::string(p.data(), p.size());
}

FetchRequest::~FetchRequest() FL_NO_EXCEPT {
    close_socket();
}

void FetchRequest::update() {
    switch (mState) {
        case DNS_LOOKUP:
            handle_dns_lookup();
            break;
        case CONNECTING:
            handle_connecting();
            break;
        case SENDING:
            handle_sending();
            break;
        case RECEIVING:
            handle_receiving();
            break;
        case COMPLETED:
        case FAILED:
            // Done - no-op
            break;
    }
}

void FetchRequest::handle_dns_lookup() {
    // Pump async system before DNS to keep server responsive
    fl::task::run(1000);

    FL_WARN_F("[FETCH] Resolving hostname: %s", mHostname);

    // DNS lookup (blocking 10-100ms, but acceptable)
    // Note: localhost is typically instant due to OS caching
    mDnsResult = gethostbyname(mHostname.c_str());

    // Pump async system after DNS to resume server pumping
    fl::task::run(1000);

    if (!mDnsResult) {
        complete_error("DNS lookup failed");
        return;
    }

    // Create socket
    mSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mSocketFd == kInvalidSocketHandle) {
        complete_error("Failed to create socket");
        return;
    }

    // Set non-blocking mode
#ifdef FL_IS_WIN
    u_long mode = 1;  // 1 = non-blocking
    ioctlsocket(mSocketFd, FIONBIO, &mode);
#else
    int flags = fcntl(mSocketFd, F_GETFL, 0);
    fcntl(mSocketFd, F_SETFL, flags | O_NONBLOCK);
#endif

    // Initiate non-blocking connect
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<u16>(mPort));
    // Use memcpy to extract address pointer — h_addr_list[0] may be misaligned
    // on some platforms (macOS arm64 UBSan flags this)
    char *addr_ptr = nullptr;
    fl::memcpy(&addr_ptr, mDnsResult->h_addr_list, sizeof(addr_ptr));
    fl::memcpy(&server_addr.sin_addr, addr_ptr, mDnsResult->h_length);

    FL_WARN_F("[FETCH] Waiting for connection to %s:%s", mHostname, mPort);

    connect(mSocketFd, (sockaddr*)&server_addr, sizeof(server_addr));
    // connect() returns immediately with EINPROGRESS/WSAEWOULDBLOCK (expected)

    mState = CONNECTING;
    mStateStartTime = fl::millis();
}

void FetchRequest::handle_connecting() {
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(mSocketFd, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;  // Non-blocking check

    // nfds is ignored by Winsock and is the highest fd + 1 on POSIX.
    int result = select(static_cast<int>(mSocketFd + 1), nullptr, &write_fds, nullptr, &timeout);

    if (result > 0) {
        // Check for connection errors
        int sock_err = 0;
        socklen_t len = sizeof(sock_err);
        getsockopt(mSocketFd, SOL_SOCKET, SO_ERROR, (char*)&sock_err, &len);

        if (sock_err != 0) {
            complete_error("Connection failed");
            return;
        }

        // Connected! Build the HTTP request from the caller's options. This
        // used to hardcode a bare GET, so fetch_post() and custom headers and
        // bodies were silently dropped on native targets.
        if (!detail::build_http_request(mOptions, mPath, mHostname, &mRequestBuffer)) {
            complete_error("Invalid request: CR or LF in the method, path, host or a header");
            return;
        }

        mBytesSent = 0;
        mState = SENDING;
        mStateStartTime = fl::millis();
    } else if (result < 0) {
        complete_error("select() failed during connection");
    } else {
        // Timeout check (5 seconds)
        if (fl::millis() - mStateStartTime > 5000) {
            complete_error("Connection timeout");
        }
        // else: Not ready yet, try again on next update()
    }
}

void FetchRequest::handle_sending() {
    ssize_t sent = send(mSocketFd,
                        mRequestBuffer.c_str() + mBytesSent,
                        mRequestBuffer.size() - mBytesSent,
                        0);

    if (sent > 0) {
        mBytesSent += sent;
        if (mBytesSent >= mRequestBuffer.size()) {
            FL_WARN_F("[FETCH] Waiting for HTTP response...");
            mState = RECEIVING;
            mStateStartTime = fl::millis();
        }
    } else if (sent < 0) {
        int err = GET_SOCKET_ERROR();
        if (err != SOCKET_ERROR_WOULD_BLOCK) {
            complete_error("Send failed");
        }
        // EWOULDBLOCK: Try again on next update()
    }
}

void FetchRequest::handle_receiving() {
    char buffer[4096];
    ssize_t bytes = recv(mSocketFd, buffer, sizeof(buffer), 0);

    if (bytes > 0) {
        mResponseBuffer.append(buffer, static_cast<size_t>(bytes));
        mStateStartTime = fl::millis();  // Reset timeout on data received
    } else if (bytes == 0) {
        // Connection closed - parse response and complete
        Response resp = parse_http_response(mResponseBuffer);
        complete_success(resp);
    } else {
        int err = GET_SOCKET_ERROR();
        if (err == SOCKET_ERROR_WOULD_BLOCK) {
            // Check timeout (5 seconds)
            if (fl::millis() - mStateStartTime > 5000) {
                complete_error("Response timeout");
            }
            // else: Not ready, try again on next update()
        } else {
            complete_error("Receive failed");
        }
    }
}

Response FetchRequest::parse_http_response(const fl::string& raw) {
    const char* data = raw.c_str();
    const size_t len = raw.size();

    // Find end of headers (double CRLF)
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == fl::string::npos) {
        return Response(500, "Internal Server Error");
    }

    // Parse status line: "HTTP/1.1 200 OK\r\n"
    size_t first_space = raw.find(' ');
    size_t second_space = raw.find(' ', first_space + 1);
    if (first_space == fl::string::npos || second_space == fl::string::npos ||
        first_space >= header_end || second_space >= header_end) {
        return Response(500, "Internal Server Error");
    }

    // Parse status code from digits without allocating
    int status_code = 0;
    for (size_t i = first_space + 1; i < second_space; ++i) {
        status_code = status_code * 10 + (data[i] - '0');
    }

    // Status text ends at first \r\n
    size_t status_line_end = raw.find("\r\n");
    size_t st_len = (status_line_end != fl::string::npos ? status_line_end : header_end) - (second_space + 1);

    Response resp(status_code, fl::string(data + second_space + 1, st_len));
    resp.set_body(fl::string(data + header_end + 4, len - header_end - 4));

    // Parse response headers using indices into raw
    size_t pos = status_line_end + 2;  // Skip past status line CRLF
    while (pos < header_end) {
        size_t line_end = raw.find("\r\n", pos);
        if (line_end == fl::string::npos || line_end > header_end) {
            line_end = header_end;
        }

        // Find colon separator
        size_t colon = fl::string::npos;
        for (size_t i = pos; i < line_end; ++i) {
            if (data[i] == ':') {
                colon = i;
                break;
            }
        }

        if (colon != fl::string::npos) {
            // Build lowercase header name
            fl::string name(data + pos, colon - pos);
            for (size_t i = 0; i < name.size(); ++i) {
                if (name[i] >= 'A' && name[i] <= 'Z') {
                    name[i] += ('a' - 'A');
                }
            }

            // Value: skip colon and optional leading space
            size_t val_start = colon + 1;
            if (val_start < line_end && data[val_start] == ' ') {
                ++val_start;
            }
            resp.set_header(name, fl::string(data + val_start, line_end - val_start));
        }

        pos = line_end + 2;
    }

    return resp;
}

void FetchRequest::complete_success(const Response& resp) {
    close_socket();
    mState = COMPLETED;

    if (mPromise.valid() && !mPromise.is_completed()) {
        mPromise.complete_with_value(resp);
    }
}

void FetchRequest::complete_error(const char* message) {
    close_socket();
    mState = FAILED;

    if (mPromise.valid() && !mPromise.is_completed()) {
        mPromise.complete_with_error(fl::task::Error(message));
    }
}

void FetchRequest::close_socket() {
    if (mSocketFd != kInvalidSocketHandle) {
        CLOSE_SOCKET(mSocketFd);
        mSocketFd = kInvalidSocketHandle;
    }
}

} // namespace http
} // namespace net
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
