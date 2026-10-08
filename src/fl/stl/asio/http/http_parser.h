#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/string.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

// Forward declaration — breaks fl.stl+ -> fl.net+ link chain
namespace fl { namespace net { namespace http { class ChunkedReader; } } }

namespace fl {

// HTTP request structure
struct HttpRequest {
    fl::string method;           // "GET", "POST", etc.
    fl::string uri;              // "/rpc"
    fl::string version;          // "HTTP/1.1"
    fl::flat_map<fl::string, fl::string, fl::StringFastLess> headers;
    fl::vector<u8> body;    // Decoded body (if chunked, already decoded)
};

// HTTP response structure
struct HttpResponse {
    fl::string version;          // "HTTP/1.1"
    int statusCode = 0;
    fl::string reasonPhrase;     // "OK", "Not Found", etc.
    fl::flat_map<fl::string, fl::string, fl::StringFastLess> headers;
    fl::vector<u8> body;    // Decoded body
};

using HttpRequestPtr = fl::shared_ptr<HttpRequest>;
using HttpResponsePtr = fl::shared_ptr<HttpResponse>;
using HttpRequestPtrConst = fl::shared_ptr<const HttpRequest>;
using HttpResponsePtrConst = fl::shared_ptr<const HttpResponse>;

// HttpRequestParser: Parse HTTP/1.1 requests
class HttpRequestParser {
public:
    HttpRequestParser() FL_NO_EXCEPT;
    ~HttpRequestParser() FL_NO_EXCEPT;

    // Feed raw bytes received from a socket into the parser (incremental/streaming)
    void feed(fl::span<const u8> data) FL_NO_EXCEPT;

    // Check if request is complete
    bool isComplete() const FL_NO_EXCEPT;

    // Get parsed request as shared_ptr (zero-copy handoff, returns null if not complete)
    HttpRequestPtrConst getRequest() FL_NO_EXCEPT;

    // Reset state
    void reset() FL_NO_EXCEPT;

    // State enum (public for debug access)
    enum State {
        READ_REQUEST_LINE,  // "POST /rpc HTTP/1.1\r\n"
        READ_HEADERS,       // "Header: Value\r\n" ... "\r\n"
        READ_BODY,          // Body content (chunked or Content-Length)
        COMPLETE            // Request fully parsed
    };

    // Debug getters (public for testing)
    State getState() const FL_NO_EXCEPT { return mState; }
    size_t getBufferSize() const FL_NO_EXCEPT { return mBuffer.size(); }
    size_t getContentLength() const FL_NO_EXCEPT { return mContentLength; }
    bool getIsChunked() const FL_NO_EXCEPT { return mIsChunked; }

private:

    State mState;
    fl::vector<u8> mBuffer;
    fl::shared_ptr<HttpRequest> mRequest;
    fl::shared_ptr<net::http::ChunkedReader> mChunkedReader;  // For Transfer-Encoding: chunked
    size_t mContentLength;
    bool mIsChunked;

    HttpRequest& req() FL_NO_EXCEPT { return *mRequest; }
    const HttpRequest& req() const FL_NO_EXCEPT { return *mRequest; }

    // Parse request line: "POST /rpc HTTP/1.1\r\n"
    bool parseRequestLine() FL_NO_EXCEPT;

    // Parse headers: "Header: Value\r\n" ... "\r\n"
    bool parseHeaders() FL_NO_EXCEPT;

    // Parse body (chunked or Content-Length)
    void parseBody() FL_NO_EXCEPT;

    // Find CRLF in buffer
    fl::optional<size_t> findCRLF() const FL_NO_EXCEPT;

    // Consume n bytes from buffer
    void consume(size_t n) FL_NO_EXCEPT;

    // Get header value (case-insensitive)
    fl::optional<fl::string> getHeader(const char* name) const FL_NO_EXCEPT;
};

// HttpResponseParser: Parse HTTP/1.1 responses
class HttpResponseParser {
public:
    HttpResponseParser() FL_NO_EXCEPT;
    ~HttpResponseParser() FL_NO_EXCEPT;

    // Feed raw bytes received from a socket into the parser (incremental/streaming)
    void feed(fl::span<const u8> data) FL_NO_EXCEPT;

    // Check if response is complete
    bool isComplete() const FL_NO_EXCEPT;

    // Get parsed response as shared_ptr (zero-copy handoff, returns null if not complete)
    HttpResponsePtrConst getResponse() FL_NO_EXCEPT;

    // Reset state
    void reset() FL_NO_EXCEPT;

    // State enum (public for debug access)
    enum State {
        READ_STATUS_LINE,   // "HTTP/1.1 200 OK\r\n"
        READ_HEADERS,       // "Header: Value\r\n" ... "\r\n"
        READ_BODY,          // Body content (chunked or Content-Length)
        COMPLETE            // Response fully parsed
    };

    // Debug getters (public for testing)
    State getState() const FL_NO_EXCEPT { return mState; }
    size_t getBufferSize() const FL_NO_EXCEPT { return mBuffer.size(); }
    size_t getContentLength() const FL_NO_EXCEPT { return mContentLength; }
    bool getIsChunked() const FL_NO_EXCEPT { return mIsChunked; }

private:

    State mState;
    fl::vector<u8> mBuffer;
    fl::shared_ptr<HttpResponse> mResponse;
    fl::shared_ptr<net::http::ChunkedReader> mChunkedReader;
    size_t mContentLength;
    bool mIsChunked;

    HttpResponse& resp() FL_NO_EXCEPT { return *mResponse; }
    const HttpResponse& resp() const FL_NO_EXCEPT { return *mResponse; }

    // Parse status line: "HTTP/1.1 200 OK\r\n"
    bool parseStatusLine() FL_NO_EXCEPT;

    // Parse headers: "Header: Value\r\n" ... "\r\n"
    bool parseHeaders() FL_NO_EXCEPT;

    // Parse body (chunked or Content-Length)
    void parseBody() FL_NO_EXCEPT;

    // Find CRLF in buffer
    fl::optional<size_t> findCRLF() const FL_NO_EXCEPT;

    // Consume n bytes from buffer
    void consume(size_t n) FL_NO_EXCEPT;

    // Get header value (case-insensitive)
    fl::optional<fl::string> getHeader(const char* name) const FL_NO_EXCEPT;
};

} // namespace fl
