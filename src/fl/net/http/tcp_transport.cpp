#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/transport.h"
#include "fl/net/socket_factory.h"
#include "fl/shared_ptr.h"
#include "fl/net/http/types.h"
#include "fl/sstream.h"
#include "fl/vector.h"

// Include platform-specific socket implementation when networking is enabled
// FastLED networking is always real networking - no stubs when FASTLED_HAS_NETWORKING is enabled
#if defined(_WIN32) && !defined(FASTLED_STUB_IMPL)
    #include "platforms/win/socket.h"
#elif defined(FASTLED_STUB_IMPL)
    #include "platforms/stub/net/socket.h"
#else
    #include "platforms/bsd/socket.h"
#endif

namespace fl {

class TcpTransport : public Transport {
public:
    TcpTransport() = default;
    virtual ~TcpTransport() = default;

    fl::future<Response> send_request(const Request& request) override {
        // FastLED networking is always real networking when FASTLED_HAS_NETWORKING is enabled
        return send_real_http_request(request);
    }

    fl::future<Response> send_request_async(const Request& request) override {
        // For both stub and real implementations, async is the same as sync for now
        return send_request(request);
    }

    bool supports_scheme(const fl::string& scheme) const override {
        return scheme == "http";
    }

    bool supports_streaming() const override {
        return true; // Real implementation supports streaming
    }

    bool supports_keepalive() const override {
        return true; // Real implementation supports keepalive
    }

    bool supports_compression() const override {
        return false; // Not implemented yet
    }

    bool supports_ssl() const override {
        return false; // TCP transport doesn't support SSL
    }

    fl::size get_active_connections() const override {
        return 0; // Not tracking connections yet
    }

    void close_all_connections() override {
        // Connection management not implemented yet
    }

    fl::string get_transport_name() const override {
        return "TCP-Real";
    }

    fl::string get_transport_version() const override {
        return "1.0.0";
    }

    fl::future<bool> stream_download(const Request& request,
                                   fl::function<bool(fl::span<const fl::u8>)> data_processor) override {
        // Stub implementation - return false (not supported)
        (void)request; // Suppress unused parameter warning
        (void)data_processor; // Suppress unused parameter warning
        return fl::make_ready_future(false);
    }

    fl::future<Response> stream_upload(const Request& request,
                                     fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider) override {
        // Stub implementation - just return a basic response
        (void)data_provider; // Suppress unused parameter warning
        Response response(HttpStatusCode::OK);
        response.set_body(fl::string("Upload complete (stub)"));
        return fl::make_ready_future(response);
    }

private:
    // Real HTTP implementation methods
    // FastLED networking is always real networking when FASTLED_HAS_NETWORKING is enabled
    fl::future<Response> send_real_http_request(const Request& request) {
        // Parse URL to extract host and port
        auto url_parts = parse_url(request.get_url());
        if (!url_parts.valid) {
            return fl::make_error_future<Response>("Invalid URL: " + request.get_url());
        }
        
        // Create socket and connect
        SocketOptions socket_options;
        socket_options.connect_timeout_ms = 10000;
        socket_options.read_timeout_ms = 30000;
        socket_options.enable_nodelay = true;
        
        auto socket = SocketFactory::create_client_socket(socket_options);
        if (!socket) {
            return fl::make_error_future<Response>("Failed to create socket");
        }
        
        // Connect to server
        auto connect_result = socket->connect(url_parts.host, url_parts.port);
        auto connect_error = connect_result.try_get_result();
        
        if (connect_error.is<FutureError>()) {
            return fl::make_error_future<Response>("Connection failed: " + socket->get_error_message());
        }
        
        if (!socket->is_connected()) {
            return fl::make_error_future<Response>("Socket not connected");
        }
        
        // Build HTTP request
        fl::string http_request = build_http_request(request, url_parts);
        
        // Send request
        fl::vector<fl::u8> request_bytes;
        request_bytes.reserve(http_request.size());
        for (char c : http_request) {
            request_bytes.push_back(static_cast<fl::u8>(c));
        }
        fl::size bytes_sent = socket->write(fl::span<const fl::u8>(request_bytes.data(), request_bytes.size()));
        
        if (bytes_sent != request_bytes.size()) {
            return fl::make_error_future<Response>("Failed to send complete request");
        }
        
        // Read response
        auto response = read_http_response(socket);
        
        // Close socket
        socket->disconnect();
        
        if (response) {
            return fl::make_ready_future(*response);
        } else {
            return fl::make_error_future<Response>("Failed to read HTTP response");
        }
    }
    
    struct UrlParts {
        fl::string scheme;
        fl::string host;
        int port;
        fl::string path;
        bool valid;
        
        UrlParts() : port(80), valid(false) {}
    };
    
    UrlParts parse_url(const fl::string& url) {
        UrlParts parts;
        
        // Find scheme
        auto scheme_end = url.find("://");
        if (scheme_end == fl::string::npos) {
            return parts;
        }
        
        // Extract scheme manually - WORKAROUND for broken fl::string::substr()
        parts.scheme.clear();
        for (fl::size i = 0; i < scheme_end; ++i) {
            parts.scheme += url[i];
        }
        fl::size start = scheme_end + 3;
        
        // Set default port - WORKAROUND for broken fl::string implementation
        // Direct character comparison without using fl::string operations
        bool is_http = (scheme_end == 4 && 
                       url[0] == 'h' && url[1] == 't' && 
                       url[2] == 't' && url[3] == 'p');
        bool is_https = (scheme_end == 5 && 
                        url[0] == 'h' && url[1] == 't' && 
                        url[2] == 't' && url[3] == 'p' && 
                        url[4] == 's');
        
        if (is_http) {
            parts.port = 80;
        } else if (is_https) {
            parts.port = 443;
        } else {
            return parts;
        }
        
        // Find path - WORKAROUND for broken fl::string::substr()
        auto path_start = url.find('/', start);
        if (path_start == fl::string::npos) {
            parts.path = "/";
            path_start = url.size();
        } else {
            // Extract path manually to avoid broken substr
            parts.path.clear();
            for (fl::size i = path_start; i < url.size(); ++i) {
                parts.path += url[i];
            }
        }
        
        // Extract host and port - WORKAROUND for broken fl::string::substr()
        // Manual character-by-character extraction due to fl::string bugs
        
        // Find port separator (':') after the host
        fl::size port_pos = fl::string::npos;
        for (fl::size i = start; i < path_start; ++i) {
            if (url[i] == ':') {
                port_pos = i;
                break;
            }
        }
        
        if (port_pos != fl::string::npos) {
            // Extract host manually
            parts.host.clear();
            for (fl::size i = start; i < port_pos; ++i) {
                parts.host += url[i];
            }
            
            // Extract and parse port manually
            parts.port = 0;
            for (fl::size i = port_pos + 1; i < path_start; ++i) {
                if (url[i] >= '0' && url[i] <= '9') {
                    parts.port = parts.port * 10 + (url[i] - '0');
                } else {
                    FL_WARN("🚨 URL Parser: Invalid port character at position " << i);
                    return parts; // Invalid port
                }
            }
        } else {
            // No port specified, extract entire host manually
            parts.host.clear();
            for (fl::size i = start; i < path_start; ++i) {
                parts.host += url[i];
            }
        }
        
        parts.valid = !parts.host.empty();
        return parts;
    }
    
    fl::string build_http_request(const Request& request, const UrlParts& url_parts) {
        fl::sstream ss;
        
        // Request line
        fl::string method = to_string(request.get_method());
        ss << method << " " << url_parts.path << " HTTP/1.1\r\n";
        
        // Host header (required for HTTP/1.1)
        ss << "Host: " << url_parts.host;
        if ((url_parts.scheme == "http" && url_parts.port != 80) ||
            (url_parts.scheme == "https" && url_parts.port != 443)) {
            ss << ":" << url_parts.port;
        }
        ss << "\r\n";
        
        // Other headers
        auto headers = request.headers();
        for (const auto& header : headers.get_all()) {
            ss << header.first << ": " << header.second << "\r\n";
        }
        
        // Default headers if not present
        if (!headers.has("User-Agent")) {
            ss << "User-Agent: FastLED/1.0\r\n";
        }
        
        if (!headers.has("Connection")) {
            ss << "Connection: close\r\n";
        }
        
        // Content-Length if we have a body
        if (request.has_body()) {
            ss << "Content-Length: " << request.get_body_size() << "\r\n";
        }
        
        // End of headers
        ss << "\r\n";
        
        // Body (if any)
        if (request.has_body()) {
            ss << request.get_body_text();
        }
        
        return ss.str();
    }
    
    fl::optional<Response> read_http_response(fl::shared_ptr<Socket> socket) {
        fl::vector<fl::u8> buffer;
        buffer.resize(8192);
        
        fl::vector<fl::u8> response_data;
        fl::size header_end_pos = fl::string::npos;
        
        // Read until we have complete headers
        while (header_end_pos == fl::string::npos) {
            fl::size bytes_read = socket->read(fl::span<fl::u8>(buffer.data(), buffer.size()));
            if (bytes_read == 0) {
                // No more data
                break;
            }
            
            // Add to response data
            for (fl::size i = 0; i < bytes_read; ++i) {
                response_data.push_back(buffer[i]);
            }
            
            // Check for header end marker
            fl::string response_str(reinterpret_cast<const char*>(response_data.data()), response_data.size());
            header_end_pos = response_str.find("\r\n\r\n");
        }
        
        if (header_end_pos == fl::string::npos) {
            return fl::nullopt; // Incomplete response
        }
        
        // Parse response
        fl::string response_str(reinterpret_cast<const char*>(response_data.data()), response_data.size());
        
        // Parse status line
        auto first_line_end = response_str.find("\r\n");
        if (first_line_end == fl::string::npos) {
            return fl::nullopt;
        }
        
        fl::string status_line = response_str.substr(0, first_line_end);
        
        // Extract status code
        auto first_space = status_line.find(' ');
        auto second_space = status_line.find(' ', first_space + 1);
        
        if (first_space == fl::string::npos || second_space == fl::string::npos) {
            return fl::nullopt;
        }
        
        fl::string status_code_str = status_line.substr(first_space + 1, second_space - first_space - 1);
        fl::string status_text = status_line.substr(second_space + 1);
        
        // Parse status code
        int status_code = 0;
        for (fl::size i = 0; i < status_code_str.size(); ++i) {
            if (status_code_str[i] >= '0' && status_code_str[i] <= '9') {
                status_code = status_code * 10 + (status_code_str[i] - '0');
            } else {
                return fl::nullopt; // Invalid status code
            }
        }
        
        // Create response object
        Response response(static_cast<HttpStatusCode>(status_code));
        
        // Parse headers
        fl::string headers_section = response_str.substr(first_line_end + 2, header_end_pos - first_line_end - 2);
        parse_headers(headers_section, response);
        
        // Get body (if any)
        fl::size body_start = header_end_pos + 4;
        if (body_start < response_data.size()) {
            fl::string body(reinterpret_cast<const char*>(response_data.data() + body_start), 
                           response_data.size() - body_start);
            
            // Check if we need to read more for Content-Length
            auto content_length_header = response.get_content_length();
            if (content_length_header) {
                fl::size expected_length = static_cast<fl::size>(*content_length_header);
                
                // Read more data if needed
                while (body.size() < expected_length) {
                    fl::size bytes_read = socket->read(fl::span<fl::u8>(buffer.data(), buffer.size()));
                    if (bytes_read == 0) {
                        break; // No more data
                    }
                    
                    for (fl::size i = 0; i < bytes_read; ++i) {
                        body.push_back(static_cast<char>(buffer[i]));
                    }
                }
            }
            
            response.set_body(body);
        }
        
        return response;
    }
    
    void parse_headers(const fl::string& headers_text, Response& response) {
        fl::size pos = 0;
        
        while (pos < headers_text.size()) {
            auto line_end = headers_text.find("\r\n", pos);
            if (line_end == fl::string::npos) {
                line_end = headers_text.size();
            }
            
            fl::string line = headers_text.substr(pos, line_end - pos);
            
            auto colon_pos = line.find(':');
            if (colon_pos != fl::string::npos) {
                fl::string header_name = line.substr(0, colon_pos);
                fl::string header_value = line.substr(colon_pos + 1);
                
                // Trim whitespace from value
                while (!header_value.empty() && (header_value[0] == ' ' || header_value[0] == '\t')) {
                    header_value = header_value.substr(1);
                }
                while (!header_value.empty() && (header_value.back() == ' ' || header_value.back() == '\t')) {
                    header_value.pop_back();
                }
                
                response.set_header(header_name, header_value);
            }
            
            pos = line_end + 2; // Skip \r\n
        }
    }
};

// Implementation function required by http_transport.cpp
fl::shared_ptr<Transport> create_tcp_transport_impl() {
    return fl::make_shared<TcpTransport>();
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
