#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking/http_client.h"
#include "fl/networking/http_transport.h"
#include "fl/networking/socket_factory.h"
#include "fl/networking/socket.h"
#include "fl/future.h"
#include "fl/string.h"
#include "fl/sstream.h"
#include "fl/vector.h"
#include "fl/shared_ptr.h"
#include "fl/time.h"

#include "fl/compiler_control.h"

namespace fl {

// ========== Simple HTTP Functions Implementation ==========

fl::future<Response> http_get(const fl::string& url) {
    auto client = HttpClient::create_simple_client();
    return client->get(url);
}

fl::future<Response> http_post(const fl::string& url, 
                               fl::span<const fl::u8> data,
                               const fl::string& content_type) {
    auto client = HttpClient::create_simple_client();
    return client->post(url, data, content_type);
}

fl::future<Response> http_post(const fl::string& url,
                               const fl::string& text,
                               const fl::string& content_type) {
    auto client = HttpClient::create_simple_client();
    return client->post(url, text, content_type);
}

fl::future<Response> http_post_json(const fl::string& url, const fl::string& json) {
    auto client = HttpClient::create_simple_client();
    return client->post(url, json, "application/json");
}

fl::future<Response> http_put(const fl::string& url, 
                              fl::span<const fl::u8> data,
                              const fl::string& content_type) {
    auto client = HttpClient::create_simple_client();
    return client->put(url, data, content_type);
}

fl::future<Response> http_delete(const fl::string& url) {
    auto client = HttpClient::create_simple_client();
    return client->delete_(url);
}

// ========== URL Parsing Helper ==========

struct ParsedUrl {
    fl::string scheme;
    fl::string host;
    int port;
    fl::string path;
    bool valid;
    
    ParsedUrl() : port(80), valid(false) {}
};

static ParsedUrl parse_url_internal(const fl::string& url) {
    ParsedUrl result;
    
    // Find scheme
    auto scheme_end = url.find("://");
    if (scheme_end == fl::string::npos) {
        return result; // Invalid URL
    }
    
    result.scheme = url.substr(0, scheme_end);
    fl::size start = scheme_end + 3;
    
    // Set default port based on scheme
    if (result.scheme == "http") {
        result.port = 80;
    } else if (result.scheme == "https") {
        result.port = 443;
    } else {
        return result; // Unsupported scheme
    }
    
    // Find host and port - search in the remaining part
    fl::string host_path_part = url.substr(start, url.size());
    auto path_start = host_path_part.find('/');
    if (path_start == fl::string::npos) {
        result.path = "/";
    } else {
        result.path = host_path_part.substr(path_start, host_path_part.size());
        path_start += start; // Adjust for full URL position
    }
    
    fl::size host_end = (path_start == fl::string::npos) ? url.size() : path_start;
    fl::string host_port = url.substr(start, host_end - start);
    auto port_start = host_port.find(':');
    if (port_start != fl::string::npos) {
        result.host = host_port.substr(0, port_start);
        fl::string port_str = host_port.substr(port_start + 1, host_port.size());
        // Simple port parsing
        result.port = 0;
        for (fl::size i = 0; i < port_str.size(); ++i) {
            if (port_str[i] >= '0' && port_str[i] <= '9') {
                result.port = result.port * 10 + (port_str[i] - '0');
            } else {
                return result; // Invalid port
            }
        }
    } else {
        result.host = host_port;
    }
    
    result.valid = !result.host.empty();
    return result;
}



// ========== HttpClient Implementation ==========

HttpClient::HttpClient(const Config& config) : mConfig(config) {
    mTransport = TransportFactory::create_tcp_transport();
    mStats = {};
}

HttpClient::HttpClient(fl::shared_ptr<Transport> transport, const Config& config) 
    : mTransport(transport), mConfig(config) {
    mStats = {};
}

HttpClient::~HttpClient() = default;

fl::future<Response> HttpClient::get(const fl::string& url) {
    Request request(HttpMethod::GET, url);
    apply_config_to_request(request);
    return send_internal(request);
}

fl::future<Response> HttpClient::post(const fl::string& url, fl::span<const fl::u8> data,
                                      const fl::string& content_type) {
    Request request(HttpMethod::POST, url);
    request.set_body(data);
    request.set_content_type(content_type);
    apply_config_to_request(request);
    return send_internal(request);
}

fl::future<Response> HttpClient::post(const fl::string& url, const fl::string& text,
                                      const fl::string& content_type) {
    Request request(HttpMethod::POST, url);
    request.set_body(text);
    request.set_content_type(content_type);
    apply_config_to_request(request);
    return send_internal(request);
}

fl::future<Response> HttpClient::send(const Request& request) {
    Request modified_request = request;
    apply_config_to_request(modified_request);
    return send_internal(modified_request);
}

fl::future<Response> HttpClient::send_async(const Request& request) {
    return send(request);
}

fl::shared_ptr<HttpClient> HttpClient::create_simple_client() {
    Config config;
    return fl::make_shared<HttpClient>(config);
}

fl::shared_ptr<HttpClient> HttpClient::create_with_tcp_transport() {
    auto transport = TransportFactory::create_tcp_transport();
    return fl::make_shared<HttpClient>(transport);
}

fl::future<Response> HttpClient::send_internal(const Request& request) {
    if (!mTransport) {
        return fl::make_error_future<Response>("No transport available");
    }
    
    if (!request.is_valid()) {
        return fl::make_error_future<Response>("Invalid request: " + request.get_validation_error());
    }
    
    // TODO: Add statistics tracking
    auto start_time = fl::time();
    auto response_future = mTransport->send_request(request);
    
    // For now, just return the future directly
    // In a real implementation, we'd add timeout handling and stats updates
    return response_future;
}

Request HttpClient::build_request(const fl::string& method, const fl::string& url) const {
    Request request;
    auto parsed_method = parse_http_method(method);
    if (parsed_method) {
        request.set_method(*parsed_method);
    }
    request.set_url(url);
    return request;
}

void HttpClient::apply_config_to_request(Request& request) const {
    // Set User-Agent if not already set
    if (!request.get_user_agent()) {
        request.set_user_agent(mConfig.user_agent);
    }
    
    // Add default headers
    for (const auto& header : mConfig.default_headers) {
        if (!request.get_header(header.first)) {
            request.set_header(header.first, header.second);
        }
    }
}



// Stub implementations for methods not yet implemented
fl::future<Response> HttpClient::put(const fl::string& url, fl::span<const fl::u8> data, const fl::string& content_type) {
    (void)url; (void)data; (void)content_type;
    return fl::make_error_future<Response>("PUT not implemented yet");
}

fl::future<Response> HttpClient::delete_(const fl::string& url) {
    (void)url;
    return fl::make_error_future<Response>("DELETE not implemented yet");
}

fl::future<Response> HttpClient::head(const fl::string& url) {
    (void)url;
    return fl::make_error_future<Response>("HEAD not implemented yet");
}

fl::future<Response> HttpClient::options(const fl::string& url) {
    (void)url;
    return fl::make_error_future<Response>("OPTIONS not implemented yet");
}

fl::future<Response> HttpClient::patch(const fl::string& url, fl::span<const fl::u8> data, const fl::string& content_type) {
    (void)url; (void)data; (void)content_type;
    return fl::make_error_future<Response>("PATCH not implemented yet");
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
