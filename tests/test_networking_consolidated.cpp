#include "test.h"
#include "fl/networking/socket.h"
#include "fl/networking/socket_factory.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/shared_ptr.h"

using namespace fl;

// Test constants
const int TEST_PORT = 18080;
const char* TEST_ADDRESS = "127.0.0.1";

// Simple stub socket for testing
class TestSocket : public Socket {
public:
    TestSocket() : mConnected(false), mHandle(1000) {}
    
    fl::future<SocketError> connect(const fl::string& host, int port) override {
        mHost = host;
        mPort = port;
        mConnected = true;
        return fl::make_ready_future(SocketError::SUCCESS);
    }
    
    fl::future<SocketError> connect_async(const fl::string& host, int port) override {
        return connect(host, port);
    }
    
    void disconnect() override { mConnected = false; }
    bool is_connected() const override { return mConnected; }
    SocketState get_state() const override {
        return mConnected ? SocketState::CONNECTED : SocketState::CLOSED;
    }
    
    fl::size read(fl::span<fl::u8> buffer) override { (void)buffer; return 0; }
    fl::size write(fl::span<const fl::u8> data) override { return data.size(); }
    fl::size available() const override { return 0; }
    void flush() override {}
    bool has_data_available() const override { return false; }
    bool can_write() const override { return mConnected; }
    
    void set_non_blocking(bool non_blocking) override { mNonBlocking = non_blocking; }
    bool is_non_blocking() const override { return mNonBlocking; }
    void set_timeout(fl::u32 timeout_ms) override { mTimeout = timeout_ms; }
    fl::u32 get_timeout() const override { return mTimeout; }
    void set_keep_alive(bool enable) override { (void)enable; }
    void set_nodelay(bool enable) override { (void)enable; }
    
    fl::string remote_address() const override { return mHost; }
    int remote_port() const override { return mPort; }
    fl::string local_address() const override { return "127.0.0.1"; }
    int local_port() const override { return 0; }
    
    SocketError get_last_error() const override { return SocketError::SUCCESS; }
    fl::string get_error_message() const override { return ""; }
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) override {
        (void)level; (void)option; (void)value; (void)value_size; return true;
    }
    
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) override {
        (void)level; (void)option; (void)value; (void)value_size; return true;
    }
    
    int get_socket_handle() const override { return mHandle; }
    
protected:
    void set_state(SocketState state) override { (void)state; }
    void set_error(SocketError error, const fl::string& message = "") override {
        (void)error; (void)message;
    }
    
private:
    bool mConnected;
    bool mNonBlocking = false;
    fl::u32 mTimeout = 5000;
    fl::string mHost;
    int mPort = 0;
    int mHandle;
};

class TestServerSocket : public ServerSocket {
public:
    TestServerSocket() : mListening(false), mPort(0) {}
    
    SocketError bind(const fl::string& address, int port) override {
        mAddress = address; mPort = port; return SocketError::SUCCESS;
    }
    
    SocketError listen(int backlog) override {
        (void)backlog; mListening = true; return SocketError::SUCCESS;
    }
    
    void close() override { mListening = false; }
    bool is_listening() const override { return mListening; }
    
    fl::shared_ptr<Socket> accept() override { return nullptr; }
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections) override {
        (void)max_connections; return fl::vector<fl::shared_ptr<Socket>>();
    }
    
    bool has_pending_connections() const override { return false; }
    void set_reuse_address(bool enable) override { (void)enable; }
    void set_reuse_port(bool enable) override { (void)enable; }
    void set_non_blocking(bool non_blocking) override { (void)non_blocking; }
    
    fl::string bound_address() const override { return mAddress; }
    int bound_port() const override { return mPort; }
    fl::size max_connections() const override { return 5; }
    fl::size current_connections() const override { return 0; }
    
    SocketError get_last_error() const override { return SocketError::SUCCESS; }
    fl::string get_error_message() const override { return ""; }
    int get_socket_handle() const override { return 2000; }
    
protected:
    void set_error(SocketError error, const fl::string& message = "") override {
        (void)error; (void)message;
    }
    
private:
    bool mListening;
    fl::string mAddress;
    int mPort;
};

// Platform-specific test implementations for testing
// These override the stub implementations during tests
fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options) {
    (void)options;
    return fl::make_shared<TestSocket>();
}

fl::shared_ptr<ServerSocket> create_platform_server_socket(const SocketOptions& options) {
    (void)options;
    return fl::make_shared<TestServerSocket>();
}

bool platform_supports_ipv6() {
    return false;  // Test implementation doesn't support IPv6
}

bool platform_supports_tls() {
    return false;  // Test implementation doesn't support TLS
}

bool platform_supports_non_blocking_connect() {
    return true;  // Test implementation supports non-blocking
}

bool platform_supports_socket_reuse() {
    return true;  // Test implementation supports socket reuse
}

TEST_CASE("Socket factory capabilities") {
    // Test platform capability detection - critical for feature negotiation
    bool ipv6_support = SocketFactory::supports_ipv6();
    bool tls_support = SocketFactory::supports_tls();
    bool non_blocking_support = SocketFactory::supports_non_blocking_connect();
    bool socket_reuse_support = SocketFactory::supports_socket_reuse();
    
    // Validate test implementation capabilities
    REQUIRE(!ipv6_support);  // Test doesn't support IPv6
    REQUIRE(!tls_support);   // Test doesn't support TLS
    REQUIRE(non_blocking_support);  // Test supports non-blocking
    REQUIRE(socket_reuse_support);  // Test supports socket reuse
}

TEST_CASE("Socket options configuration") {
    SocketOptions options;
    
    // Validate default configuration
    REQUIRE_EQ(IpVersion::AUTO, options.ip_version);
    REQUIRE(options.enable_keepalive);
    REQUIRE(options.enable_nodelay);
    REQUIRE_EQ(10000u, options.connect_timeout_ms);
    REQUIRE_EQ(5000u, options.read_timeout_ms);
    REQUIRE_EQ(5000u, options.write_timeout_ms);
    REQUIRE_EQ(8192u, options.buffer_size);
    REQUIRE(options.enable_reuse_addr);
    REQUIRE(!options.enable_reuse_port);
    
    // Test configuration modification
    options.ip_version = IpVersion::IPV4_ONLY;
    options.enable_keepalive = false;
    options.connect_timeout_ms = 15000;
    
    REQUIRE_EQ(IpVersion::IPV4_ONLY, options.ip_version);
    REQUIRE(!options.enable_keepalive);
    REQUIRE_EQ(15000u, options.connect_timeout_ms);
    
    // Validate hash function works
    fl::size hash_value = options.hash();
    REQUIRE_GT(hash_value, 0u);
}

TEST_CASE("Socket enum validation") {
    // Critical for API contract validation
    REQUIRE_EQ(0, static_cast<int>(SocketError::SUCCESS));
    REQUIRE_NE(static_cast<int>(SocketError::SUCCESS), static_cast<int>(SocketError::CONNECTION_FAILED));
    REQUIRE_NE(static_cast<int>(SocketState::CLOSED), static_cast<int>(SocketState::CONNECTED));
    REQUIRE_NE(static_cast<int>(IpVersion::IPV4_ONLY), static_cast<int>(IpVersion::IPV6_ONLY));
}

TEST_CASE("Socket creation with platform functions") {
    // Validate that platform functions provide valid sockets
    auto client_socket = SocketFactory::create_client_socket();
    auto server_socket = SocketFactory::create_server_socket();
    
    REQUIRE(client_socket != nullptr);
    REQUIRE(server_socket != nullptr);
}

TEST_CASE("Complete networking integration") {
    // Register test factories for end-to-end testing
    // SocketFactory::register_platform_factory(
    //     [](const SocketOptions& options) -> fl::shared_ptr<Socket> {
    //         (void)options; return fl::make_shared<TestSocket>();
    //     },
    //     [](const SocketOptions& options) -> fl::shared_ptr<ServerSocket> {
    //         (void)options; return fl::make_shared<TestServerSocket>();
    //     }
    // );
    
    // Create client and server sockets
    auto client = SocketFactory::create_client_socket();
    auto server = SocketFactory::create_server_socket();
    
    REQUIRE(client);
    REQUIRE(server);
    
    // Test initial states
    REQUIRE(!client->is_connected());
    REQUIRE_EQ(SocketState::CLOSED, client->get_state());
    REQUIRE(!server->is_listening());
    
    // Set up server
    REQUIRE_EQ(SocketError::SUCCESS, server->bind(TEST_ADDRESS, TEST_PORT));
    REQUIRE_EQ(fl::string(TEST_ADDRESS), server->bound_address());
    REQUIRE_EQ(TEST_PORT, server->bound_port());
    
    REQUIRE_EQ(SocketError::SUCCESS, server->listen(1));
    REQUIRE(server->is_listening());
    
    // Connect client
    auto connect_future = client->connect(TEST_ADDRESS, TEST_PORT);
    auto connect_result = connect_future.try_get_result();
    REQUIRE(connect_result.is<SocketError>());
    REQUIRE_EQ(SocketError::SUCCESS, *connect_result.ptr<SocketError>());
    REQUIRE(client->is_connected());
    REQUIRE_EQ(SocketState::CONNECTED, client->get_state());
    REQUIRE_EQ(fl::string(TEST_ADDRESS), client->remote_address());
    REQUIRE_EQ(TEST_PORT, client->remote_port());
    
    // Test data transfer
    fl::string test_data = "Hello Network";
    fl::vector<fl::u8> data_bytes;
    for (fl::size i = 0; i < test_data.size(); i++) {
        data_bytes.push_back(static_cast<fl::u8>(test_data[i]));
    }
    
    fl::size bytes_written = client->write(data_bytes);
    REQUIRE_EQ(data_bytes.size(), bytes_written);
    
    // Test socket configuration
    client->set_timeout(10000);
    REQUIRE_EQ(10000u, client->get_timeout());
    
    client->set_non_blocking(true);
    REQUIRE(client->is_non_blocking());
    
    // Test socket properties
    REQUIRE(client->can_write());
    REQUIRE_EQ(0u, client->available());
    REQUIRE(!client->has_data_available());
    
    // Test socket handles
    REQUIRE_GT(client->get_socket_handle(), 0);
    REQUIRE_GT(server->get_socket_handle(), 0);
    
    // Clean up
    client->disconnect();
    server->close();
    
    REQUIRE(!client->is_connected());
    REQUIRE(!server->is_listening());
} 
