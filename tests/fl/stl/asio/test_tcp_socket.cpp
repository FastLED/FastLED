#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/asio/error_code.cpp.hpp"
#include "fl/stl/asio/ip/tcp.h"
#include "fl/stl/asio/ip/tcp.cpp.hpp"

#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl::asio;
using namespace fl::asio::ip;

FL_TEST_CASE("tcp::socket - Default construction is closed") {
    tcp::socket s;
    FL_CHECK_FALSE(s.is_open());
    FL_CHECK_EQ(s.native_handle(), -1);
    FL_CHECK_FALSE(s.is_non_blocking());
}

FL_TEST_CASE("tcp::socket - Move construction") {
    tcp::socket a;
    a.assign(42);
    FL_CHECK(a.is_open());
    FL_CHECK_EQ(a.native_handle(), 42);

    tcp::socket b(fl::move(a));
    FL_CHECK(b.is_open());
    FL_CHECK_EQ(b.native_handle(), 42);
    FL_CHECK_FALSE(a.is_open());

    // Prevent destructor from closing invalid fd
    b.assign(-1);
}

FL_TEST_CASE("tcp::socket - Move assignment") {
    tcp::socket a;
    a.assign(43);
    tcp::socket b;
    b = fl::move(a);
    FL_CHECK(b.is_open());
    FL_CHECK_EQ(b.native_handle(), 43);
    FL_CHECK_FALSE(a.is_open());

    // Prevent destructor from closing invalid fd
    b.assign(-1);
}

FL_TEST_CASE("tcp::socket - Close resets state") {
    tcp::socket s;
    s.assign(44);
    FL_CHECK(s.is_open());
    s.assign(-1);
    FL_CHECK_FALSE(s.is_open());
}

FL_TEST_CASE("tcp::socket - Read on closed socket returns error") {
    tcp::socket s;
    fl::u8 buf[16];
    error_code ec;
    size_t n = s.read_some(fl::span<fl::u8>(buf, sizeof(buf)), ec);
    FL_CHECK_EQ(n, 0u);
    FL_CHECK(static_cast<bool>(ec));
    FL_CHECK_EQ(ec.code, errc::operation_aborted);
}

FL_TEST_CASE("tcp::socket - Write on closed socket returns error") {
    tcp::socket s;
    const fl::u8 data[] = {1, 2, 3};
    error_code ec;
    size_t n = s.write_some(fl::span<const fl::u8>(data, sizeof(data)), ec);
    FL_CHECK_EQ(n, 0u);
    FL_CHECK(static_cast<bool>(ec));
    FL_CHECK_EQ(ec.code, errc::operation_aborted);
}

FL_TEST_CASE("tcp::socket - Connect to invalid host returns error") {
    tcp::socket s;
    tcp::endpoint ep("this.host.does.not.exist.invalid", 9999);
    error_code ec = s.connect(ep);
    FL_CHECK(static_cast<bool>(ec));
    FL_CHECK_FALSE(s.is_open());
}

FL_TEST_CASE("tcp::socket - Loopback connect and I/O") {
    // Create server acceptor on ephemeral port (port 0)
    tcp::acceptor acc;
    error_code ec = acc.open(0);
    FL_CHECK(ec.ok());
    FL_CHECK(acc.is_open());

    ec = acc.listen(1);
    FL_CHECK(ec.ok());

    // Get the actual assigned port
    fl::u16 port = acc.port();
    FL_CHECK(port > 0);

    // Client connects to loopback on the actual port
    tcp::socket client;
    ec = client.connect(tcp::endpoint("127.0.0.1", port));
    FL_CHECK(ec.ok());
    FL_CHECK(client.is_open());

    // Server accepts (with retry loop for non-blocking socket)
    tcp::socket server_peer;
    for (int attempt = 0; attempt < 100; ++attempt) {
        ec = acc.accept(server_peer);
        if (ec.code != errc::would_block) {
            break;
        }
    }
    FL_CHECK(ec.ok());
    FL_CHECK(server_peer.is_open());

    // Client writes, server reads
    const fl::u8 msg[] = "hello";
    size_t written = client.write_some(fl::span<const fl::u8>(msg, 5), ec);
    FL_CHECK(ec.ok());
    FL_CHECK_EQ(written, 5u);

    fl::u8 buf[64] = {};
    size_t total_read = 0;
    for (int attempt = 0; attempt < 100 && total_read < 5; ++attempt) {
        size_t n = server_peer.read_some(
            fl::span<fl::u8>(buf + total_read, sizeof(buf) - total_read), ec);
        if (ec.code == errc::would_block) {
            continue;
        }
        if (!ec.ok()) {
            FL_WARN("read_some failed: errc=" << static_cast<int>(ec.code)
                    << " msg=" << ec.message
                    << " n=" << n
                    << " attempt=" << attempt
                    << " total_read=" << total_read);
        }
        FL_CHECK(ec.ok());
        total_read += n;
    }
    if (total_read != 5) {
        FL_WARN("Loopback read incomplete: total_read=" << total_read
                << " (expected 5)");
    }
    FL_CHECK_EQ(total_read, 5u);
    FL_CHECK_EQ(buf[0], 'h');
    FL_CHECK_EQ(buf[4], 'o');

    client.close();
    server_peer.close();
    acc.close();
}

#endif // FASTLED_HAS_NETWORKING

} // FL_TEST_FILE
