#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/optional.h"
#include "fl/stl/string.h"
#include "fl/stl/map.h"
#include "fl/stl/stdint.h"
#include "fl/stl/asio/http/chunked_encoding.h"

namespace fl {

// HTTP request structure
struct HttpRequest {
    fl::string method;           // "GET", "POST", etc.
    fl::string uri;              // "/rpc"
    fl::string version;          // "HTTP/1.1"
    fl::map<fl::string, fl::string> headers;
    fl::vector<u8> body;    // Decoded body (if chunked, already decoded)
};

// HTTP response structure
struct HttpResponse {
    fl::string version;          // "HTTP/1.1"
    int statusCode = 0;
    fl::string reasonPhrase;     // "OK", "Not Found", etc.
    fl::map<fl::string, fl::string> headers;
    fl::vector<u8> body;    // Decoded body
};

using HttpRequestPtr = fl::shared_ptr<HttpRequest>;
using HttpResponsePtr = fl::shared_ptr<HttpResponse>;
using HttpRequestPtrConst = fl::shared_ptr<const HttpRequest>;
using HttpResponsePtrConst = fl::shared_ptr<const HttpResponse>;

// HttpRequestParser: Parse HTTP/1.1 requests
class HttpRequestParser {
public:
    HttpRequestParser();

    // Feed raw bytes received from a socket into the parser (incremental/streaming)
    void feed(fl::span<const u8> data);

    // Check if request is complete
    bool isComplete() const;

    // Get parsed request as shared_ptr (zero-copy handoff, returns null if not complete)
    HttpRequestPtrConst getRequest();

    // Reset state
    void reset();

    // State enum (public for debug access)
    enum State {
        READ_REQUEST_LINE,  // "POST /rpc HTTP/1.1\r\n"
        READ_HEADERS,       // "Header: Value\r\n" ... "\r\n"
        READ_BODY,          // Body content (chunked or Content-Length)
        COMPLETE            // Request fully parsed
    };

    // Debug getters (public for testing)
    State getState() const { return mState; }
    size_t getBufferSize() const { return mBuffer.size(); }
    size_t getContentLength() const { return mContentLength; }
    bool getIsChunked() const { return mIsChunked; }

private:

    State mState;
    fl::vector<u8> mBuffer;
    fl::shared_ptr<HttpRequest> mRequest;
    ChunkedReader mChunkedReader;  // For Transfer-Encoding: chunked
    size_t mContentLength;
    bool mIsChunked;

    HttpRequest& req() { return *mRequest; }
    const HttpRequest& req() const { return *mRequest; }

    // Parse request line: "POST /rpc HTTP/1.1\r\n"
    bool parseRequestLine();

    // Parse headers: "Header: Value\r\n" ... "\r\n"
    bool parseHeaders();

    // Parse body (chunked or Content-Length)
    void parseBody();

    // Find CRLF in buffer
    fl::optional<size_t> findCRLF() const;

    // Consume n bytes from buffer
    void consume(size_t n);

    // Get header value (case-insensitive)
    fl::optional<fl::string> getHeader(const char* name) const;
};

// HttpResponseParser: Parse HTTP/1.1 responses
class HttpResponseParser {
public:
    HttpResponseParser();

    // Feed raw bytes received from a socket into the parser (incremental/streaming)
    void feed(fl::span<const u8> data);

    // Check if response is complete
    bool isComplete() const;

    // Get parsed response as shared_ptr (zero-copy handoff, returns null if not complete)
    HttpResponsePtrConst getResponse();

    // Reset state
    void reset();

    // State enum (public for debug access)
    enum State {
        READ_STATUS_LINE,   // "HTTP/1.1 200 OK\r\n"
        READ_HEADERS,       // "Header: Value\r\n" ... "\r\n"
        READ_BODY,          // Body content (chunked or Content-Length)
        COMPLETE            // Response fully parsed
    };

    // Debug getters (public for testing)
    State getState() const { return mState; }
    size_t getBufferSize() const { return mBuffer.size(); }
    size_t getContentLength() const { return mContentLength; }
    bool getIsChunked() const { return mIsChunked; }

private:

    State mState;
    fl::vector<u8> mBuffer;
    fl::shared_ptr<HttpResponse> mResponse;
    ChunkedReader mChunkedReader;
    size_t mContentLength;
    bool mIsChunked;

    HttpResponse& resp() { return *mResponse; }
    const HttpResponse& resp() const { return *mResponse; }

    // Parse status line: "HTTP/1.1 200 OK\r\n"
    bool parseStatusLine();

    // Parse headers: "Header: Value\r\n" ... "\r\n"
    bool parseHeaders();

    // Parse body (chunked or Content-Length)
    void parseBody();

    // Find CRLF in buffer
    fl::optional<size_t> findCRLF() const;

    // Consume n bytes from buffer
    void consume(size_t n);

    // Get header value (case-insensitive)
    fl::optional<fl::string> getHeader(const char* name) const;
};

} // namespace fl
