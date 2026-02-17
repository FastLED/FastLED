#pragma once

#include "fl/net/fetch_request.h"
#include "fl/async.h"
#include "fl/warn.h"
#include "fl/int.h"

// IWYU pragma: begin_keep
#include <cstring>  // For memcpy
// IWYU pragma: end_keep

#ifdef FASTLED_HAS_NETWORKING

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

    // Save Arduino INPUT macro before including Windows headers
    #ifdef INPUT
        #define ARDUINO_INPUT_BACKUP INPUT
        #undef INPUT
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

    // Restore Arduino INPUT macro after Windows headers
    #ifdef ARDUINO_INPUT_BACKUP
        #define INPUT ARDUINO_INPUT_BACKUP
        #undef ARDUINO_INPUT_BACKUP
    #endif

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
namespace net {

FetchRequest::FetchRequest(const fl::string& url, const fetch_options& opts, fl::promise<response> prom)
    : state(DNS_LOOKUP)
    , promise(prom)
    , hostname()
    , port(80)
    , path("/")
    , protocol("http")
    , socket_fd(-1)
    , dns_result(nullptr)
    , request_buffer()
    , response_buffer()
    , bytes_sent(0)
    , state_start_time(fl::millis())
{
    if (!parse_url(url)) {
        complete_error("Invalid URL");
    }
}

FetchRequest::~FetchRequest() {
    close_socket();
}

bool FetchRequest::parse_url(const fl::string& url) {
    // Find protocol
    size_t proto_end = url.find("://");
    if (proto_end == fl::string::npos) {
        return false;
    }

    protocol = url.substr(0, proto_end);
    size_t host_start = proto_end + 3;

    // Find path
    size_t path_start = url.find('/', host_start);
    fl::string host_port;

    if (path_start != fl::string::npos) {
        host_port = url.substr(host_start, path_start - host_start);
        path = url.substr(path_start);
    } else {
        host_port = url.substr(host_start);
        path = "/";
    }

    // Split host and port
    size_t port_sep = host_port.find(':');
    if (port_sep != fl::string::npos) {
        hostname = host_port.substr(0, port_sep);
        fl::string port_str = host_port.substr(port_sep + 1);
        port = atoi(port_str.c_str());
    } else {
        hostname = host_port;
        port = (protocol == "https") ? 443 : 80;
    }

    return !hostname.empty();
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
    fl::async_run();

    FL_WARN("[FETCH] Resolving hostname: " << hostname);

    // DNS lookup (blocking 10-100ms, but acceptable)
    // Note: localhost is typically instant due to OS caching
    dns_result = gethostbyname(hostname.c_str());

    // Pump async system after DNS to resume server pumping
    fl::async_run();

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
    ::memcpy(&server_addr.sin_addr, dns_result->h_addr, dns_result->h_length);

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
    // Find end of headers (double CRLF)
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == fl::string::npos) {
        return response(500, "Internal Server Error");
    }

    fl::string headers = raw.substr(0, header_end);
    fl::string body = raw.substr(header_end + 4);

    // Parse status line (e.g., "HTTP/1.1 200 OK")
    size_t first_space = headers.find(' ');
    size_t second_space = headers.find(' ', first_space + 1);

    if (first_space == fl::string::npos || second_space == fl::string::npos) {
        return response(500, "Internal Server Error");
    }

    fl::string status_str = headers.substr(first_space + 1, second_space - first_space - 1);
    int status_code = atoi(status_str.c_str());

    // Extract status text (e.g., "OK" from "HTTP/1.1 200 OK")
    fl::string status_text = headers.substr(second_space + 1);
    size_t status_text_end = status_text.find("\r\n");
    if (status_text_end != fl::string::npos) {
        status_text = status_text.substr(0, status_text_end);
    }

    response resp(status_code, status_text);
    resp.set_body(body);

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

} // namespace net
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
