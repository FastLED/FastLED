#pragma once

#include "fl/stl/asio/fetch_request.h"
#include "fl/stl/async.h"
#include "fl/warn.h"
#include "fl/int.h"

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
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    // IWYU pragma: end_keep
    #define SOCKET_ERROR_WOULD_BLOCK EWOULDBLOCK
    #define GET_SOCKET_ERROR() errno
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace fl {
namespace asio {

FetchRequest::FetchRequest(const fl::string& url, const fetch_options& opts, fl::promise<response> prom)
    : state(DNS_LOOKUP)
    , promise(prom)
    , parsed_url(url)
    , hostname()
    , port(80)
    , path("/")
    , socket_fd(-1)
    , dns_result(nullptr)
    , request_buffer()
    , response_buffer()
    , bytes_sent(0)
    , state_start_time(fl::millis())
{
    if (!parsed_url.isValid() || parsed_url.host().empty()) {
        complete_error("Invalid URL");
        return;
    }
    hostname = fl::string(parsed_url.host().data(), parsed_url.host().size());
    port = parsed_url.port();
    fl::string_view p = parsed_url.path();
    path = p.empty() ? fl::string("/") : fl::string(p.data(), p.size());
}

FetchRequest::~FetchRequest() {
    close_socket();
}

void FetchRequest::update() {
    switch (state) {
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
    fl::async_run(1000);

    FL_WARN("[FETCH] Resolving hostname: " << hostname);

    // DNS lookup (blocking 10-100ms, but acceptable)
    // Note: localhost is typically instant due to OS caching
    dns_result = gethostbyname(hostname.c_str());

    // Pump async system after DNS to resume server pumping
    fl::async_run(1000);

    if (!dns_result) {
        complete_error("DNS lookup failed");
        return;
    }

    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        complete_error("Failed to create socket");
        return;
    }

    // Set non-blocking mode
#ifdef FL_IS_WIN
    u_long mode = 1;  // 1 = non-blocking
    ioctlsocket(socket_fd, FIONBIO, &mode);
#else
    int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif

    // Initiate non-blocking connect
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<u16>(port));
    // Use memcpy to extract address pointer — h_addr_list[0] may be misaligned
    // on some platforms (macOS arm64 UBSan flags this)
    char *addr_ptr = nullptr;
    fl::memcpy(&addr_ptr, dns_result->h_addr_list, sizeof(addr_ptr));
    fl::memcpy(&server_addr.sin_addr, addr_ptr, dns_result->h_length);

    FL_WARN("[FETCH] Waiting for connection to " << hostname << ":" << port);

    connect(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
    // connect() returns immediately with EINPROGRESS/WSAEWOULDBLOCK (expected)

    state = CONNECTING;
    state_start_time = fl::millis();
}

void FetchRequest::handle_connecting() {
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(socket_fd, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;  // Non-blocking check

    int result = select(socket_fd + 1, nullptr, &write_fds, nullptr, &timeout);

    if (result > 0) {
        // Check for connection errors
        int sock_err = 0;
        socklen_t len = sizeof(sock_err);
        getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, (char*)&sock_err, &len);

        if (sock_err != 0) {
            complete_error("Connection failed");
            return;
        }

        // Connected! Build HTTP request
        request_buffer = "GET " + path + " HTTP/1.1\r\n";
        request_buffer += "Host: " + hostname + "\r\n";
        request_buffer += "Connection: close\r\n\r\n";

        bytes_sent = 0;
        state = SENDING;
        state_start_time = fl::millis();
    } else if (result < 0) {
        complete_error("select() failed during connection");
    } else {
        // Timeout check (5 seconds)
        if (fl::millis() - state_start_time > 5000) {
            complete_error("Connection timeout");
        }
        // else: Not ready yet, try again on next update()
    }
}

void FetchRequest::handle_sending() {
    ssize_t sent = send(socket_fd,
                        request_buffer.c_str() + bytes_sent,
                        request_buffer.size() - bytes_sent,
                        0);

    if (sent > 0) {
        bytes_sent += sent;
        if (bytes_sent >= request_buffer.size()) {
            FL_WARN("[FETCH] Waiting for HTTP response...");
            state = RECEIVING;
            state_start_time = fl::millis();
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
    ssize_t bytes = recv(socket_fd, buffer, sizeof(buffer), 0);

    if (bytes > 0) {
        response_buffer.append(buffer, static_cast<size_t>(bytes));
        state_start_time = fl::millis();  // Reset timeout on data received
    } else if (bytes == 0) {
        // Connection closed - parse response and complete
        response resp = parse_http_response(response_buffer);
        complete_success(resp);
    } else {
        int err = GET_SOCKET_ERROR();
        if (err == SOCKET_ERROR_WOULD_BLOCK) {
            // Check timeout (5 seconds)
            if (fl::millis() - state_start_time > 5000) {
                complete_error("Response timeout");
            }
            // else: Not ready, try again on next update()
        } else {
            complete_error("Receive failed");
        }
    }
}

response FetchRequest::parse_http_response(const fl::string& raw) {
    const char* data = raw.c_str();
    const size_t len = raw.size();

    // Find end of headers (double CRLF)
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == fl::string::npos) {
        return response(500, "Internal Server Error");
    }

    // Parse status line: "HTTP/1.1 200 OK\r\n"
    size_t first_space = raw.find(' ');
    size_t second_space = raw.find(' ', first_space + 1);
    if (first_space == fl::string::npos || second_space == fl::string::npos ||
        first_space >= header_end || second_space >= header_end) {
        return response(500, "Internal Server Error");
    }

    // Parse status code from digits without allocating
    int status_code = 0;
    for (size_t i = first_space + 1; i < second_space; ++i) {
        status_code = status_code * 10 + (data[i] - '0');
    }

    // Status text ends at first \r\n
    size_t status_line_end = raw.find("\r\n");
    size_t st_len = (status_line_end != fl::string::npos ? status_line_end : header_end) - (second_space + 1);

    response resp(status_code, fl::string(data + second_space + 1, st_len));
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

void FetchRequest::complete_success(const response& resp) {
    close_socket();
    state = COMPLETED;

    if (promise.valid() && !promise.is_completed()) {
        promise.complete_with_value(resp);
    }
}

void FetchRequest::complete_error(const char* message) {
    close_socket();
    state = FAILED;

    if (promise.valid() && !promise.is_completed()) {
        promise.complete_with_error(Error(message));
    }
}

void FetchRequest::close_socket() {
    if (socket_fd >= 0) {
        CLOSE_SOCKET(socket_fd);
        socket_fd = -1;
    }
}

} // namespace asio
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
