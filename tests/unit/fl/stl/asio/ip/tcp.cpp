#include "fl/stl/asio/ip/tcp.h"

#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::asio::ip::tcp;

FL_TEST_CASE("endpoint - Default construction") {
    endpoint ep;
    FL_CHECK(ep.host.empty());
    FL_CHECK_EQ(ep.port, 0);
}

FL_TEST_CASE("endpoint - Construction with host and port") {
    endpoint ep("localhost", 8080);
    FL_CHECK_EQ(ep.host, fl::string("localhost"));
    FL_CHECK_EQ(ep.port, 8080);
}

FL_TEST_CASE("endpoint - Construction with fl::string") {
    fl::string host("192.168.1.1");
    endpoint ep(host, 443);
    FL_CHECK_EQ(ep.host, fl::string("192.168.1.1"));
    FL_CHECK_EQ(ep.port, 443);
}

FL_TEST_CASE("endpoint - Equality comparison") {
    endpoint a("localhost", 8080);
    endpoint b("localhost", 8080);
    endpoint c("localhost", 9090);
    endpoint d("other", 8080);

    FL_CHECK(a == b);
    FL_CHECK(a != c);
    FL_CHECK(a != d);
}

FL_TEST_CASE("endpoint - Copy semantics") {
    endpoint a("example.com", 80);
    endpoint b = a;
    FL_CHECK(a == b);
    b.port = 443;
    FL_CHECK(a != b);
}

} // FL_TEST_FILE
