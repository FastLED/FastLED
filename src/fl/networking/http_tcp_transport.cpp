#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking/http_transport.h"
#include "fl/networking/socket_factory.h"
#include "fl/shared_ptr.h"
#include "fl/networking/http_types.h"

namespace fl {

class TcpTransport : public Transport {
public:
    TcpTransport() = default;
    virtual ~TcpTransport() = default;

    fl::future<Response> send_request(const Request& request) override {
        // For stub implementation, return a simple response
        Response response(HttpStatusCode::OK);
        response.set_body(fl::string("Stub HTTP response from TCP transport"));
        response.set_header("Content-Type", "text/plain");
        response.set_header("Server", "FastLED-TCP-Stub");
        
        return fl::make_ready_future(response);
    }

    fl::future<Response> send_request_async(const Request& request) override {
        // For stub implementation, async is the same as sync
        return send_request(request);
    }

    bool supports_scheme(const fl::string& scheme) const override {
        return scheme == "http";
    }

    bool supports_streaming() const override {
        return false; // Stub implementation doesn't support streaming
    }

    bool supports_keepalive() const override {
        return false; // Stub implementation doesn't support keepalive
    }

    bool supports_compression() const override {
        return false; // Stub implementation doesn't support compression
    }

    bool supports_ssl() const override {
        return false; // TCP transport doesn't support SSL
    }

    fl::size get_active_connections() const override {
        return 0; // Stub implementation has no active connections
    }

    void close_all_connections() override {
        // Nothing to close in stub implementation
    }

    fl::string get_transport_name() const override {
        return "TCP-Stub";
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
};

// Implementation function required by http_transport.cpp
fl::shared_ptr<Transport> create_tcp_transport_impl() {
    return fl::make_shared<TcpTransport>();
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
