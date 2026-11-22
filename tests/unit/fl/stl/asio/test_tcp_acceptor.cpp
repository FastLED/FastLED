
#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/asio/error_code.h"
#include "fl/stl/asio/ip/tcp.h"
#include "fl/stl/thread.h"
#include "fl/stl/chrono.h"

#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl::asio;
using namespace fl::asio::ip;

FL_TEST_CASE("tcp::acceptor - Default construction") {
    tcp::acceptor acc;
    FL_CHECK_FALSE(acc.is_open());
    FL_CHECK_EQ(acc.native_handle(), -1);
    FL_CHECK_EQ(acc.port(), 0);
}

FL_TEST_CASE("tcp::acceptor - Open binds to port") {
    tcp::acceptor acc;
    error_code ec = acc.open(0);
    FL_CHECK(ec.ok());
    FL_CHECK(acc.is_open());
    FL_CHECK(acc.port() > 0);
    acc.close();
    FL_CHECK_FALSE(acc.is_open());
}

FL_TEST_CASE("tcp::acceptor - Listen on unopened acceptor fails") {
    tcp::acceptor acc;
    error_code ec = acc.listen();
    FL_CHECK(static_cast<bool>(ec));
    FL_CHECK_EQ(ec.code, errc::operation_aborted);
}

FL_TEST_CASE("tcp::acceptor - Accept with no connection returns would_block") {
    tcp::acceptor acc;
    error_code ec = acc.open(0);
    FL_CHECK(ec.ok());
    ec = acc.listen(1);
    FL_CHECK(ec.ok());

    tcp::socket peer;
    ec = acc.accept(peer);
    FL_CHECK_EQ(ec.code, errc::would_block);
    FL_CHECK_FALSE(peer.is_open());

    acc.close();
}

FL_TEST_CASE("tcp::acceptor - Close resets state") {
    tcp::acceptor acc;
    error_code ec = acc.open(0);
    FL_CHECK(ec.ok());
    fl::u16 p = acc.port();
    FL_CHECK(p > 0);

    acc.close();
    FL_CHECK_FALSE(acc.is_open());
    FL_CHECK_EQ(acc.port(), 0);
}

FL_TEST_CASE("tcp::acceptor - Multiple clients") {
    tcp::acceptor acc;
    error_code ec = acc.open(0);
    FL_CHECK(ec.ok());
    ec = acc.listen(5);
    FL_CHECK(ec.ok());

    fl::u16 port = acc.port();

    // Connect two clients
    tcp::socket client1, client2;
    ec = client1.connect(tcp::endpoint("127.0.0.1", port));
    FL_CHECK(ec.ok());
    ec = client2.connect(tcp::endpoint("127.0.0.1", port));
    FL_CHECK(ec.ok());

    // Accept both
    tcp::socket peer1, peer2;

    bool accepted1 = false, accepted2 = false;
    for (int i = 0; i < 100 && (!accepted1 || !accepted2); ++i) {
        if (!accepted1) {
            ec = acc.accept(peer1);
            if (ec.ok()) {
                accepted1 = true;
            }
        }
        if (!accepted2) {
            ec = acc.accept(peer2);
            if (ec.ok()) {
                accepted2 = true;
            }
        }
        // Give the kernel time to complete the TCP handshake on loopback
        if (!accepted1 || !accepted2) {
            fl::this_thread::sleep_for(fl::chrono::milliseconds(1));  // ok sleep for - blocking retry in test
        }
    }

    FL_CHECK(accepted1);
    FL_CHECK(accepted2);
    FL_CHECK(peer1.is_open());
    FL_CHECK(peer2.is_open());

    client1.close();
    client2.close();
    peer1.close();
    peer2.close();
    acc.close();
}

#endif // FASTLED_HAS_NETWORKING

} // FL_TEST_FILE
