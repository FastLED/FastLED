// Native fetch() must send the caller's method, headers and body.
//
// FetchRequest used to ignore its FetchOptions entirely and always send a
// bare "GET <path>", so fetch_post(), custom headers and request bodies were
// silently dropped on native targets. These cases drive the real fetch path
// against a real local fl::HttpServer.

#include "test.h"

#include "fl/net/http/fetch.h"
#include "fl/net/http/fetch_request.h"
#include "fl/stl/asio/http/server.h"
#include "fl/task/executor.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

// Distinct from the ports other HTTP tests use (48001+).
const int kPort = 48917;

// Pump the fetch and the server together until the fetch settles. The server
// only makes progress on update(), so a plain await_top_level would stall.
fl::task::PromiseResult<fl::net::http::Response> pump(
    fl::task::Promise<fl::net::http::Response> promise, fl::HttpServer& server) {
    for (int i = 0; i < 5000 && !promise.is_completed(); ++i) {
        promise.update();
        fl::task::run(1000);
        server.update();
    }
    if (!promise.is_completed()) {
        return fl::task::PromiseResult<fl::net::http::Response>(
            fl::task::Error("fetch did not complete"));
    }
    if (promise.is_resolved()) {
        return fl::task::PromiseResult<fl::net::http::Response>(promise.value());
    }
    return fl::task::PromiseResult<fl::net::http::Response>(promise.error());
}

}  // namespace

FL_TEST_CASE("fetch_post sends its method, headers and body") {
    fl::HttpServer server;
    fl::string seen;
    server.post("/echo", [&seen](const fl::http_request& req) {
        fl::optional<fl::string> tag = req.header("X-Test-Tag");
        seen = req.method() + "|" + (tag ? *tag : fl::string("<none>")) + "|" +
               req.body();
        return fl::http_response::ok(seen);
    });
    FL_REQUIRE(server.start(kPort));

    fl::net::http::FetchOptions options("");
    options.header("X-Test-Tag", "abc").body("hello body");
    auto result = pump(
        fl::net::http::fetch_post("http://localhost:48917/echo", options), server);

    FL_REQUIRE_MESSAGE(result.ok(), result.error_message());
    FL_CHECK_EQ(result.value().status(), 200);
    FL_CHECK_EQ(seen, fl::string("POST|abc|hello body"));
    FL_CHECK_EQ(result.value().text(), fl::string("POST|abc|hello body"));
    server.stop();
}

FL_TEST_CASE("build_http_request: method, headers, then framing from the body") {
    fl::net::http::RequestOptions options("POST");
    options.headers["X-Tag"] = "abc";
    options.body = "hello";
    FL_CHECK_EQ(fl::net::http::detail::build_http_request(options, "/p", "h.example"),
                fl::string("POST /p HTTP/1.1\r\n"
                           "Host: h.example\r\n"
                           "X-Tag: abc\r\n"
                           "Content-Length: 5\r\n"
                           "Connection: close\r\n\r\n"
                           "hello"));
}

FL_TEST_CASE("build_http_request: a caller cannot send its own framing headers") {
    // Content-Length / Transfer-Encoding (any case) would duplicate or
    // contradict the framing derived from the body; Host and Connection are
    // written by FetchRequest too.
    fl::net::http::RequestOptions options("POST");
    options.headers["content-length"] = "999";
    options.headers["Transfer-Encoding"] = "chunked";
    options.headers["HOST"] = "evil.example";
    options.headers["Connection"] = "keep-alive";
    options.body = "abc";
    FL_CHECK_EQ(fl::net::http::detail::build_http_request(options, "/", "h.example"),
                fl::string("POST / HTTP/1.1\r\n"
                           "Host: h.example\r\n"
                           "Content-Length: 3\r\n"
                           "Connection: close\r\n\r\n"
                           "abc"));
}

FL_TEST_CASE("build_http_request: empty method and body give a bare GET") {
    fl::net::http::RequestOptions options("");
    FL_CHECK_EQ(fl::net::http::detail::build_http_request(options, "/", "h.example"),
                fl::string("GET / HTTP/1.1\r\n"
                           "Host: h.example\r\n"
                           "Connection: close\r\n\r\n"));
}

FL_TEST_CASE("fetch_get still sends a plain GET") {
    fl::HttpServer server;
    fl::string method;
    server.get("/ping", [&method](const fl::http_request& req) {
        method = req.method();
        return fl::http_response::ok("pong");
    });
    FL_REQUIRE(server.start(kPort));

    auto result = pump(fl::net::http::fetch_get("http://localhost:48917/ping"), server);

    FL_REQUIRE_MESSAGE(result.ok(), result.error_message());
    FL_CHECK_EQ(method, fl::string("GET"));
    FL_CHECK_EQ(result.value().text(), fl::string("pong"));
    server.stop();
}

}  // FL_TEST_FILE
