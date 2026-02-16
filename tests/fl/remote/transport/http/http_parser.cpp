#include "fl/remote/transport/http/http_parser.h"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"

#include "test.h"
#include "fl/stl/cstring.h"

// Helper to get string length
static size_t str_len(const char* s) {
    return fl::strlen(s);
}

using namespace fl;

FL_TEST_CASE("HttpRequestParser - Simple GET request") {
    HttpRequestParser parser;

    const char* request =
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request), str_len(request));

    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    FL_CHECK(req->method == "GET");
    FL_CHECK(req->uri == "/hello");
    FL_CHECK(req->version == "HTTP/1.1");
    FL_CHECK(req->headers.size() == 1);
    FL_CHECK(req->headers["Host"] == "localhost");
    FL_CHECK(req->body.empty());
}

FL_TEST_CASE("HttpRequestParser - POST with Content-Length") {
    HttpRequestParser parser;

    const char* request =
        "POST /rpc HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "{\"test\": 123}";  // Fixed: Added space to match Content-Length: 13

    parser.feed(reinterpret_cast<const uint8_t*>(request), str_len(request));

    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    FL_CHECK(req->method == "POST");
    FL_CHECK(req->uri == "/rpc");
    FL_CHECK(req->version == "HTTP/1.1");
    FL_CHECK(req->headers.size() == 3);
    FL_CHECK(req->headers["Content-Type"] == "application/json");
    FL_CHECK(req->headers["Content-Length"] == "13");
    FL_CHECK(req->body.size() == 13);

    fl::string body(reinterpret_cast<const char*>(req->body.data()), req->body.size());
    FL_CHECK(body == "{\"test\": 123}");
}

FL_TEST_CASE("HttpRequestParser - POST with chunked encoding") {
    HttpRequestParser parser;

    const char* request =
        "POST /rpc HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "7\r\n"
        "{\"test\"\r\n"
        "6\r\n"
        ": 123}\r\n"  // Fixed: Added space to make chunk exactly 6 bytes
        "0\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request), str_len(request));

    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    FL_CHECK(req->method == "POST");
    FL_CHECK(req->uri == "/rpc");
    FL_CHECK(req->headers["Transfer-Encoding"] == "chunked");
    FL_CHECK(req->body.size() == 13);

    fl::string body(reinterpret_cast<const char*>(req->body.data()), req->body.size());
    FL_CHECK(body == "{\"test\": 123}");
}

FL_TEST_CASE("HttpRequestParser - Incremental parsing") {
    HttpRequestParser parser;

    // Feed request in small chunks
    const char* part1 = "POST /rpc";
    const char* part2 = " HTTP/1.1\r\n";
    const char* part3 = "Content-Length: 5\r\n";
    const char* part4 = "\r\n";
    const char* part5 = "hello";

    parser.feed(reinterpret_cast<const uint8_t*>(part1), str_len(part1));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part2), str_len(part2));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part3), str_len(part3));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part4), str_len(part4));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part5), str_len(part5));
    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    FL_CHECK(req->method == "POST");
    FL_CHECK(req->uri == "/rpc");

    fl::string body(reinterpret_cast<const char*>(req->body.data()), req->body.size());
    FL_CHECK(body == "hello");
}

FL_TEST_CASE("HttpRequestParser - Case-insensitive headers") {
    HttpRequestParser parser;

    const char* request =
        "GET / HTTP/1.1\r\n"
        "content-type: text/html\r\n"
        "Content-Length: 0\r\n"
        "TRANSFER-ENCODING: identity\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request), str_len(request));

    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    FL_CHECK(req->headers.size() == 3);
}

FL_TEST_CASE("HttpRequestParser - Multiple headers with same name") {
    HttpRequestParser parser;

    const char* request =
        "GET / HTTP/1.1\r\n"
        "Accept: text/html\r\n"
        "Accept: application/json\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request), str_len(request));

    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    // Note: Current implementation stores last value
    FL_CHECK(req->headers["Accept"] == "application/json");
}

FL_TEST_CASE("HttpRequestParser - Reset after parsing") {
    HttpRequestParser parser;

    const char* request1 =
        "GET /first HTTP/1.1\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request1), str_len(request1));
    FL_CHECK(parser.isComplete());

    auto req1 = parser.getRequest();
    FL_REQUIRE(req1.has_value());
    FL_CHECK(req1->uri == "/first");

    // Parser should auto-reset after getRequest()
    FL_CHECK_FALSE(parser.isComplete());

    const char* request2 =
        "GET /second HTTP/1.1\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request2), str_len(request2));
    FL_CHECK(parser.isComplete());

    auto req2 = parser.getRequest();
    FL_REQUIRE(req2.has_value());
    FL_CHECK(req2->uri == "/second");
}

FL_TEST_CASE("HttpResponseParser - Simple 200 OK response") {
    HttpResponseParser parser;

    const char* response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(response), str_len(response));

    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->version == "HTTP/1.1");
    FL_CHECK(res->statusCode == 200);
    FL_CHECK(res->reasonPhrase == "OK");
    FL_CHECK(res->headers.size() == 1);
    FL_CHECK(res->headers["Content-Type"] == "application/json");
    FL_CHECK(res->body.empty());
}

FL_TEST_CASE("HttpResponseParser - 404 Not Found") {
    HttpResponseParser parser;

    const char* response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Not found";

    parser.feed(reinterpret_cast<const uint8_t*>(response), str_len(response));

    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->statusCode == 404);
    FL_CHECK(res->reasonPhrase == "Not Found");
    FL_CHECK(res->body.size() == 9);

    fl::string body(reinterpret_cast<const char*>(res->body.data()), res->body.size());
    FL_CHECK(body == "Not found");
}

FL_TEST_CASE("HttpResponseParser - Response with chunked encoding") {
    HttpResponseParser parser;

    const char* response =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello\r\n"
        "6\r\n"
        " world\r\n"
        "0\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(response), str_len(response));

    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->statusCode == 200);
    FL_CHECK(res->headers["Transfer-Encoding"] == "chunked");
    FL_CHECK(res->body.size() == 11);

    fl::string body(reinterpret_cast<const char*>(res->body.data()), res->body.size());
    FL_CHECK(body == "hello world");
}

FL_TEST_CASE("HttpResponseParser - Incremental parsing") {
    HttpResponseParser parser;

    const char* part1 = "HTTP/1.1 ";
    const char* part2 = "200 OK\r\n";
    const char* part3 = "Content-Length: 3\r\n";
    const char* part4 = "\r\n";
    const char* part5 = "abc";

    parser.feed(reinterpret_cast<const uint8_t*>(part1), str_len(part1));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part2), str_len(part2));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part3), str_len(part3));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part4), str_len(part4));
    FL_CHECK_FALSE(parser.isComplete());

    parser.feed(reinterpret_cast<const uint8_t*>(part5), str_len(part5));
    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->statusCode == 200);

    fl::string body(reinterpret_cast<const char*>(res->body.data()), res->body.size());
    FL_CHECK(body == "abc");
}

FL_TEST_CASE("HttpResponseParser - Status code without reason phrase") {
    HttpResponseParser parser;

    const char* response =
        "HTTP/1.1 204\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(response), str_len(response));

    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->statusCode == 204);
    FL_CHECK(res->reasonPhrase.empty());
}

FL_TEST_CASE("HttpResponseParser - Reset after parsing") {
    HttpResponseParser parser;

    const char* response1 =
        "HTTP/1.1 200 OK\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(response1), str_len(response1));
    FL_CHECK(parser.isComplete());

    auto res1 = parser.getResponse();
    FL_REQUIRE(res1.has_value());
    FL_CHECK(res1->statusCode == 200);

    // Parser should auto-reset after getResponse()
    FL_CHECK_FALSE(parser.isComplete());

    const char* response2 =
        "HTTP/1.1 404 Not Found\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(response2), str_len(response2));
    FL_CHECK(parser.isComplete());

    auto res2 = parser.getResponse();
    FL_REQUIRE(res2.has_value());
    FL_CHECK(res2->statusCode == 404);
}

FL_TEST_CASE("HttpResponseParser - Large chunked response") {
    HttpResponseParser parser;

    const char* response =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "100\r\n";  // 256 bytes

    parser.feed(reinterpret_cast<const uint8_t*>(response), str_len(response));

    // Create 256-byte chunk
    fl::vector<uint8_t> largeChunk(256, 'A');
    parser.feed(largeChunk.data(), largeChunk.size());

    const char* trailer = "\r\n0\r\n\r\n";
    parser.feed(reinterpret_cast<const uint8_t*>(trailer), str_len(trailer));

    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->body.size() == 256);
    FL_CHECK(res->body[0] == 'A');
    FL_CHECK(res->body[255] == 'A');
}

FL_TEST_CASE("HttpRequestParser - Header value trimming") {
    HttpRequestParser parser;

    const char* request =
        "GET / HTTP/1.1\r\n"
        "Host:   localhost   \r\n"
        "Accept:text/html\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(request), str_len(request));

    FL_CHECK(parser.isComplete());

    auto req = parser.getRequest();
    FL_REQUIRE(req.has_value());
    FL_CHECK(req->headers["Host"] == "localhost");
    FL_CHECK(req->headers["Accept"] == "text/html");
}

FL_TEST_CASE("HttpResponseParser - Header value trimming") {
    HttpResponseParser parser;

    const char* response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type:  application/json  \r\n"
        "Server:nginx\r\n"
        "\r\n";

    parser.feed(reinterpret_cast<const uint8_t*>(response), str_len(response));

    FL_CHECK(parser.isComplete());

    auto res = parser.getResponse();
    FL_REQUIRE(res.has_value());
    FL_CHECK(res->headers["Content-Type"] == "application/json");
    FL_CHECK(res->headers["Server"] == "nginx");
}
