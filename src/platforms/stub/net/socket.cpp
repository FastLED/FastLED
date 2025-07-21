#if defined(FASTLED_HAS_NETWORKING)
// Stub sockets are the "real" implementation for the stub platform
// They provide mock networking functionality for testing purposes

#include "socket.h"

#include "fl/shared_ptr.h"

namespace fl {

// Static counter for generating unique socket handles
static int s_next_socket_handle = 1000;
static int s_next_server_socket_handle = 2000;

//=============================================================================
// StubSocket Implementation
//=============================================================================

StubSocket::StubSocket(const SocketOptions& options) 
    : mOptions(options), mSocketHandle(generate_socket_handle()), mTimeout(options.read_timeout_ms) {
}

fl::future<SocketError> StubSocket::connect(const fl::string& host, int port) {
    mConnectionAttempts++;
    
    if (mLastError != SocketError::SUCCESS) {
        return fl::make_ready_future(mLastError);
    }
    
    if (mConnectionDelay > 0) {
        simulate_network_delay();
    }
    
    mRemoteHost = host;
    mRemotePort = port;
    set_state(SocketState::CONNECTED);
    
    return fl::make_ready_future(SocketError::SUCCESS);
}

fl::future<SocketError> StubSocket::connect_async(const fl::string& host, int port) {
    // For stub implementation, async connect is the same as regular connect
    return connect(host, port);
}

void StubSocket::disconnect() {
    set_state(SocketState::CLOSED);
    mRemoteHost = "";
    mRemotePort = 0;
    
    // Disconnect from peer if connected
    if (mPeer) {
        auto peer = mPeer;
        mPeer.reset();
        peer->mPeer.reset();
    }
}

bool StubSocket::is_connected() const {
    return mState == SocketState::CONNECTED;
}

SocketState StubSocket::get_state() const {
    return mState;
}

fl::size StubSocket::read(fl::span<fl::u8> buffer) {
    if (!is_connected()) {
        return 0;
    }
    
    fl::size bytes_to_read = 0;
    
    // Read from mock response first
    while (bytes_to_read < buffer.size() && !mMockResponse.empty()) {
        buffer[bytes_to_read] = mMockResponse.front();
        mMockResponse.pop_front();
        bytes_to_read++;
    }
    
    // Read from receive buffer (peer data)
    while (bytes_to_read < buffer.size() && !mReceiveBuffer.empty()) {
        buffer[bytes_to_read] = mReceiveBuffer.front();
        mReceiveBuffer.pop_front();
        bytes_to_read++;
    }
    
    mBytesReceived += bytes_to_read;
    return calculate_transfer_rate(bytes_to_read);
}

fl::size StubSocket::write(fl::span<const fl::u8> data) {
    if (!is_connected()) {
        return 0;
    }
    
    fl::size bytes_written = calculate_transfer_rate(data.size());
    
    // Store sent data for inspection
    for (fl::size i = 0; i < bytes_written; i++) {
        mSentData.push_back(data[i]);
    }
    
    // Write to peer if connected
    if (mPeer && mPeer->is_connected()) {
        write_to_peer(data.slice(0, bytes_written));
    }
    
    mBytesSent += bytes_written;
    return bytes_written;
}

fl::size StubSocket::available() const {
    return mMockResponse.size() + mReceiveBuffer.size();
}

void StubSocket::flush() {
    // Nothing to flush in stub implementation
}

bool StubSocket::has_data_available() const {
    return available() > 0;
}

bool StubSocket::can_write() const {
    return is_connected();
}

void StubSocket::set_non_blocking(bool non_blocking) {
    mIsNonBlocking = non_blocking;
}

bool StubSocket::is_non_blocking() const {
    return mIsNonBlocking;
}

void StubSocket::set_timeout(fl::u32 timeout_ms) {
    mTimeout = timeout_ms;
}

fl::u32 StubSocket::get_timeout() const {
    return mTimeout;
}

void StubSocket::set_keep_alive(bool enable) {
    // Stub implementation - just store the setting
    (void)enable;
}

void StubSocket::set_nodelay(bool enable) {
    // Stub implementation - just store the setting
    (void)enable;
}

fl::string StubSocket::remote_address() const {
    return mRemoteHost;
}

int StubSocket::remote_port() const {
    return mRemotePort;
}

fl::string StubSocket::local_address() const {
    return mLocalAddress;
}

int StubSocket::local_port() const {
    return mLocalPort;
}

SocketError StubSocket::get_last_error() const {
    return mLastError;
}

fl::string StubSocket::get_error_message() const {
    return mErrorMessage;
}

bool StubSocket::set_socket_option(int level, int option, const void* value, fl::size value_size) {
    // Stub implementation - just return success
    (void)level; (void)option; (void)value; (void)value_size;
    return true;
}

bool StubSocket::get_socket_option(int level, int option, void* value, fl::size* value_size) {
    // Stub implementation - just return success
    (void)level; (void)option; (void)value; (void)value_size;
    return true;
}

int StubSocket::get_socket_handle() const {
    return mSocketHandle;
}

void StubSocket::set_mock_response(fl::span<const fl::u8> data) {
    mMockResponse.clear();
    for (fl::size i = 0; i < data.size(); i++) {
        mMockResponse.push_back(data[i]);
    }
}

void StubSocket::set_mock_error(SocketError error, const fl::string& message) {
    set_error(error, message);
}

void StubSocket::set_mock_connection_delay(fl::u32 delay_ms) {
    mConnectionDelay = delay_ms;
}

void StubSocket::simulate_connection_loss() {
    set_state(SocketState::ERROR);
    set_error(SocketError::CONNECTION_FAILED, "Simulated connection loss");
}

void StubSocket::simulate_slow_network(fl::u32 bytes_per_second) {
    mBytesPerSecond = bytes_per_second;
}

fl::vector<fl::u8> StubSocket::get_sent_data() const {
    fl::vector<fl::u8> result;
    for (const auto& byte : mSentData) {
        result.push_back(byte);
    }
    return result;
}

fl::size StubSocket::get_bytes_sent() const {
    return mBytesSent;
}

fl::size StubSocket::get_bytes_received() const {
    return mBytesReceived;
}

fl::size StubSocket::get_connection_attempts() const {
    return mConnectionAttempts;
}

void StubSocket::connect_to_peer(fl::shared_ptr<StubSocket> peer) {
    mPeer = peer;
    // Note: For proper bidirectional connection, this should be called from outside
    // where both shared_ptrs are available
}

void StubSocket::set_state(SocketState state) {
    mState = state;
}

void StubSocket::set_error(SocketError error, const fl::string& message) {
    mLastError = error;
    mErrorMessage = message;
    if (error != SocketError::SUCCESS) {
        set_state(SocketState::ERROR);
    }
}

void StubSocket::simulate_network_delay() {
    // Stub implementation - could add actual delay for more realistic testing
}

fl::size StubSocket::calculate_transfer_rate(fl::size requested_bytes) {
    if (mBytesPerSecond == 0) {
        return requested_bytes;  // Unlimited
    }
    
    // For stub implementation, just return requested bytes
    // In a real implementation, this would account for time and rate limiting
    return requested_bytes;
}

void StubSocket::write_to_peer(fl::span<const fl::u8> data) {
    if (mPeer) {
        for (fl::size i = 0; i < data.size(); i++) {
            mPeer->mReceiveBuffer.push_back(data[i]);
        }
    }
}

int StubSocket::generate_socket_handle() {
    return s_next_socket_handle++;
}

//=============================================================================
// Platform-specific socket creation functions (required by socket_factory.cpp)
//=============================================================================

fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options) {
    (void)options;
    return fl::make_shared<StubSocket>();
}

// Platform capability queries  
bool platform_supports_ipv6() {
    return false; // Stub implementation
}

bool platform_supports_tls() {
    return false; // Stub implementation
}

bool platform_supports_non_blocking_connect() {
    return false; // Stub implementation
}

bool platform_supports_socket_reuse() {
    return false; // Stub implementation
}

} // namespace fl

#endif // defined(FASTLED_HAS_NETWORKING) && defined(FASTLED_STUB_IMPL)
