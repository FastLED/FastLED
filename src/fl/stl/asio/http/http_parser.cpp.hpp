#pragma once

#include "fl/stl/asio/http/http_parser.h"
// IWYU pragma: begin_keep
#include "fl/net/http/chunked_encoding.h"  // Full type for shared_ptr<ChunkedReader>
// IWYU pragma: end_keep
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/stl/cctype.h"
#include "fl/stl/noexcept.h"
namespace fl {
namespace {

// Helper: Convert string to lowercase
fl::string toLower(const fl::string& str) {
    fl::string result = str;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] = fl::tolower(result[i]);
    }
    return result;
}

// Helper: Trim whitespace from string
fl::string http_parser_trim(const fl::string& str) {
    if (str.empty()) return str;

    size_t start = 0;
    while (start < str.size() && fl::isspace(str[start])) {
        start++;
    }

    size_t end = str.size();
    while (end > start && fl::isspace(str[end - 1])) {
        end--;
    }

    return str.substr(start, end - start);
}

// Helper: Parse integer from string
bool parseInt(const fl::string& str, int& out) {
    if (str.empty()) return false;

    int value = 0;
    for (size_t i = 0; i < str.size(); i++) {
        if (!fl::isdigit(str[i])) {
            return false;
        }
        value = value * 10 + (str[i] - '0');
    }

    out = value;
    return true;
}
} // anonymous namespace

//==============================================================================
// HttpRequestParser
//==============================================================================

HttpRequestParser::HttpRequestParser()
    : mState(READ_REQUEST_LINE)
    , mRequest(fl::make_shared<HttpRequest>())
    , mChunkedReader(fl::make_shared<net::http::ChunkedReader>())
    , mContentLength(0)
    , mIsChunked(false)
{
}

HttpRequestParser::~HttpRequestParser() FL_NOEXCEPT = default;

void HttpRequestParser::feed(fl::span<const u8> data) {
    mBuffer.insert(mBuffer.end(), data.begin(), data.end());

    // State machine: parse incrementally
    bool progress = true;
    while (progress && mState != COMPLETE) {
        progress = false;

        switch (mState) {
            case READ_REQUEST_LINE:

                if (parseRequestLine()) {

                    mState = READ_HEADERS;
                    progress = true;
                }
                break;

            case READ_HEADERS:

                if (parseHeaders()) {

                    // Check if body is expected
                    auto contentLength = getHeader("Content-Length");
                    auto transferEncoding = getHeader("Transfer-Encoding");

                    if (transferEncoding.has_value() &&
                        toLower(transferEncoding.value()).find("chunked") != fl::string::npos) {
                        mIsChunked = true;
                        mState = READ_BODY;
                    } else if (contentLength.has_value()) {
                        int contentLengthInt = 0;
                        if (parseInt(contentLength.value(), contentLengthInt)) {
                            mContentLength = static_cast<size_t>(contentLengthInt);
                            mState = READ_BODY;
                        } else {
                            // Invalid Content-Length, skip to complete
                            mState = COMPLETE;
                        }
                    } else {
                        // No body
                        mState = COMPLETE;
                    }
                    progress = true;
                }
                break;

            case READ_BODY:

                parseBody();
                // parseBody() will set mState to COMPLETE when done
                // Continue processing if state changed to COMPLETE
                if (mState == COMPLETE) {

                    progress = true;
                }
                break;

            case COMPLETE:
                // Nothing to do
                break;
        }
    }
}

bool HttpRequestParser::isComplete() const {
    return mState == COMPLETE;
}

HttpRequestPtrConst HttpRequestParser::getRequest() {
    if (mState != COMPLETE) {
        return HttpRequestPtr();
    }

    // Hand off the shared_ptr (zero-copy) and allocate a fresh one for next parse
    HttpRequestPtr result = mRequest;
    reset();
    return result;
}

void HttpRequestParser::reset() {
    mState = READ_REQUEST_LINE;
    mBuffer.clear();
    mRequest = fl::make_shared<HttpRequest>();
    mChunkedReader->reset();
    mContentLength = 0;
    mIsChunked = false;
}

bool HttpRequestParser::parseRequestLine() {
    auto crlfPos = findCRLF();
    if (!crlfPos.has_value()) {
        return false;  // Need more data
    }

    // Extract request line
    fl::string line(reinterpret_cast<const char*>(mBuffer.data()), crlfPos.value()); // ok reinterpret cast

    consume(crlfPos.value() + 2);  // +2 for CRLF

    // Parse: "METHOD URI VERSION"
    size_t methodEnd = line.find(' ');
    if (methodEnd == fl::string::npos) {
        return false;  // Invalid format
    }

    size_t uriEnd = line.find(' ', methodEnd + 1);
    if (uriEnd == fl::string::npos) {
        return false;  // Invalid format
    }

    req().method = line.substr(0, methodEnd);
    req().uri = line.substr(methodEnd + 1, uriEnd - methodEnd - 1);
    req().version = line.substr(uriEnd + 1);

    return true;
}

bool HttpRequestParser::parseHeaders() {
    while (true) {
        auto crlfPos = findCRLF();
        if (!crlfPos.has_value()) {
            return false;  // Need more data
        }

        // Check for empty line (end of headers)
        if (crlfPos.value() == 0) {
            consume(2);  // Consume final CRLF
            return true;
        }

        // Extract header line
        fl::string line(reinterpret_cast<const char*>(mBuffer.data()), crlfPos.value()); // ok reinterpret cast
        consume(crlfPos.value() + 2);  // +2 for CRLF

        // Parse: "Name: Value"
        size_t colonPos = line.find(':');
        if (colonPos == fl::string::npos) {
            continue;  // Skip invalid header
        }

        fl::string name = http_parser_trim(line.substr(0, colonPos));
        fl::string value = http_parser_trim(line.substr(colonPos + 1));

        req().headers[name] = value;
    }
}

void HttpRequestParser::parseBody() {
    if (mIsChunked) {
        // Feed buffer to chunked reader
        if (!mBuffer.empty()) {
            mChunkedReader->feed(mBuffer);
            mBuffer.clear();
        }

        // Read all available chunks
        while (mChunkedReader->hasChunk()) {
            size_t chunkSz = mChunkedReader->nextChunkSize();
            size_t offset = req().body.size();
            req().body.resize(offset + chunkSz);
            auto result = mChunkedReader->readChunk(
                fl::span<u8>(req().body.data() + offset, chunkSz));
            (void)result;
        }

        // Check if final chunk received
        if (mChunkedReader->isFinal()) {
            mState = COMPLETE;
        }
    } else {
        // Read Content-Length bytes
        if (mBuffer.size() >= mContentLength) {
            req().body.insert(req().body.end(),
                              mBuffer.begin(),
                              mBuffer.begin() + mContentLength);
            consume(mContentLength);
            mState = COMPLETE;
        }
    }
}

fl::optional<size_t> HttpRequestParser::findCRLF() const {
    for (size_t i = 0; i + 1 < mBuffer.size(); i++) {
        if (mBuffer[i] == '\r' && mBuffer[i + 1] == '\n') {
            return i;
        }
    }
    return fl::nullopt;
}

void HttpRequestParser::consume(size_t n) {
    if (n >= mBuffer.size()) {
        mBuffer.clear();
    } else {
        // Work around fl::vector::erase() issue by copying remaining data
        fl::vector<u8> remaining(mBuffer.begin() + n, mBuffer.end());
        mBuffer = remaining;
    }
}

fl::optional<fl::string> HttpRequestParser::getHeader(const char* name) const {
    fl::string lowerName = toLower(name);

    for (const auto& pair : req().headers) {
        if (toLower(pair.first) == lowerName) {
            return pair.second;
        }
    }

    return fl::nullopt;
}

//==============================================================================
// HttpResponseParser
//==============================================================================

HttpResponseParser::HttpResponseParser()
    : mState(READ_STATUS_LINE)
    , mResponse(fl::make_shared<HttpResponse>())
    , mChunkedReader(fl::make_shared<net::http::ChunkedReader>())
    , mContentLength(0)
    , mIsChunked(false)
{
}

HttpResponseParser::~HttpResponseParser() FL_NOEXCEPT = default;

void HttpResponseParser::feed(fl::span<const u8> data) {
    mBuffer.insert(mBuffer.end(), data.begin(), data.end());

    // State machine: parse incrementally
    bool progress = true;
    while (progress && mState != COMPLETE) {
        progress = false;

        switch (mState) {
            case READ_STATUS_LINE:

                if (parseStatusLine()) {

                    mState = READ_HEADERS;
                    progress = true;
                }
                break;

            case READ_HEADERS:

                if (parseHeaders()) {

                    // Check if body is expected
                    auto contentLength = getHeader("Content-Length");
                    auto transferEncoding = getHeader("Transfer-Encoding");

                    if (transferEncoding.has_value() &&
                        toLower(transferEncoding.value()).find("chunked") != fl::string::npos) {
                        mIsChunked = true;
                        mState = READ_BODY;
                    } else if (contentLength.has_value()) {
                        int contentLengthInt = 0;
                        if (parseInt(contentLength.value(), contentLengthInt)) {
                            mContentLength = static_cast<size_t>(contentLengthInt);
                            mState = READ_BODY;
                        } else {
                            // Invalid Content-Length, skip to complete
                            mState = COMPLETE;
                        }
                    } else {
                        // No body
                        mState = COMPLETE;
                    }
                    progress = true;
                }
                break;

            case READ_BODY:

                parseBody();
                // parseBody() will set mState to COMPLETE when done
                // Continue processing if state changed to COMPLETE
                if (mState == COMPLETE) {

                    progress = true;
                }
                break;

            case COMPLETE:
                // Nothing to do
                break;
        }
    }
}

bool HttpResponseParser::isComplete() const {
    return mState == COMPLETE;
}

HttpResponsePtrConst HttpResponseParser::getResponse() {
    if (mState != COMPLETE) {
        return HttpResponsePtr();
    }

    // Hand off the shared_ptr (zero-copy) and allocate a fresh one for next parse
    HttpResponsePtr result = mResponse;
    reset();
    return result;
}

void HttpResponseParser::reset() {
    mState = READ_STATUS_LINE;
    mBuffer.clear();
    mResponse = fl::make_shared<HttpResponse>();
    mChunkedReader->reset();
    mContentLength = 0;
    mIsChunked = false;
}

bool HttpResponseParser::parseStatusLine() {
    auto crlfPos = findCRLF();
    if (!crlfPos.has_value()) {
        return false;  // Need more data
    }

    // Extract status line
    fl::string line(reinterpret_cast<const char*>(mBuffer.data()), crlfPos.value()); // ok reinterpret cast
    consume(crlfPos.value() + 2);  // +2 for CRLF

    // Parse: "VERSION STATUS_CODE REASON_PHRASE"
    size_t versionEnd = line.find(' ');
    if (versionEnd == fl::string::npos) {
        return false;  // Invalid format
    }

    size_t statusEnd = line.find(' ', versionEnd + 1);
    if (statusEnd == fl::string::npos) {
        // No reason phrase (optional in HTTP/1.1)
        statusEnd = line.size();
    }

    resp().version = line.substr(0, versionEnd);
    fl::string statusStr = line.substr(versionEnd + 1, statusEnd - versionEnd - 1);

    if (!parseInt(statusStr, resp().statusCode)) {
        return false;  // Invalid status code
    }

    if (statusEnd < line.size()) {
        resp().reasonPhrase = line.substr(statusEnd + 1);
    }

    return true;
}

bool HttpResponseParser::parseHeaders() {
    while (true) {
        auto crlfPos = findCRLF();
        if (!crlfPos.has_value()) {
            return false;  // Need more data
        }

        // Check for empty line (end of headers)
        if (crlfPos.value() == 0) {
            consume(2);  // Consume final CRLF
            return true;
        }

        // Extract header line
        fl::string line(reinterpret_cast<const char*>(mBuffer.data()), crlfPos.value()); // ok reinterpret cast
        consume(crlfPos.value() + 2);  // +2 for CRLF

        // Parse: "Name: Value"
        size_t colonPos = line.find(':');
        if (colonPos == fl::string::npos) {
            continue;  // Skip invalid header
        }

        fl::string name = http_parser_trim(line.substr(0, colonPos));
        fl::string value = http_parser_trim(line.substr(colonPos + 1));

        resp().headers[name] = value;
    }
}

void HttpResponseParser::parseBody() {
    if (mIsChunked) {
        // Feed buffer to chunked reader
        if (!mBuffer.empty()) {
            mChunkedReader->feed(mBuffer);
            mBuffer.clear();
        }

        // Read all available chunks
        while (mChunkedReader->hasChunk()) {
            size_t chunkSz = mChunkedReader->nextChunkSize();
            size_t offset = resp().body.size();
            resp().body.resize(offset + chunkSz);
            auto result = mChunkedReader->readChunk(
                fl::span<u8>(resp().body.data() + offset, chunkSz));
            (void)result;
        }

        // Check if final chunk received
        if (mChunkedReader->isFinal()) {
            mState = COMPLETE;
        }
    } else {
        // Read Content-Length bytes
        if (mBuffer.size() >= mContentLength) {
            resp().body.insert(resp().body.end(),
                               mBuffer.begin(),
                               mBuffer.begin() + mContentLength);
            consume(mContentLength);
            mState = COMPLETE;
        }
    }
}

fl::optional<size_t> HttpResponseParser::findCRLF() const {
    for (size_t i = 0; i + 1 < mBuffer.size(); i++) {
        if (mBuffer[i] == '\r' && mBuffer[i + 1] == '\n') {
            return i;
        }
    }
    return fl::nullopt;
}

void HttpResponseParser::consume(size_t n) {
    if (n >= mBuffer.size()) {
        mBuffer.clear();
    } else {
        // Work around fl::vector::erase() issue by copying remaining data
        fl::vector<u8> remaining(mBuffer.begin() + n, mBuffer.end());
        mBuffer = remaining;
    }
}

fl::optional<fl::string> HttpResponseParser::getHeader(const char* name) const {
    fl::string lowerName = toLower(name);

    for (const auto& pair : resp().headers) {
        if (toLower(pair.first) == lowerName) {
            return pair.second;
        }
    }

    return fl::nullopt;
}

} // namespace fl
