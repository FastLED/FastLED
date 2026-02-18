#pragma once

#include "fl/remote/transport/http/http_parser.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/stl/cctype.h"
namespace fl {

// Helper: Convert string to lowercase
static fl::string toLower(const fl::string& str) {
    fl::string result = str;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] = static_cast<char>(::tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

// Helper: Trim whitespace from string
static fl::string trim(const fl::string& str) {
    if (str.empty()) return str;

    size_t start = 0;
    while (start < str.size() && ::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }

    size_t end = str.size();
    while (end > start && ::isspace(static_cast<unsigned char>(str[end - 1]))) {
        end--;
    }

    return str.substr(start, end - start);
}

// Helper: Parse integer from string
static bool parseInt(const fl::string& str, int& out) {
    if (str.empty()) return false;

    int value = 0;
    for (size_t i = 0; i < str.size(); i++) {
        if (!::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
        value = value * 10 + (str[i] - '0');
    }

    out = value;
    return true;
}

//==============================================================================
// HttpRequestParser
//==============================================================================

HttpRequestParser::HttpRequestParser()
    : mState(READ_REQUEST_LINE)
    , mContentLength(0)
    , mIsChunked(false)
{
}

void HttpRequestParser::feed(const u8* data, size_t len) {
    mBuffer.insert(mBuffer.end(), data, data + len);

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

fl::optional<HttpRequest> HttpRequestParser::getRequest() {
    if (mState != COMPLETE) {
        return fl::nullopt;
    }

    
    HttpRequest result = mRequest;
    
    reset();
    return result;
}

void HttpRequestParser::reset() {
    mState = READ_REQUEST_LINE;
    mBuffer.clear();
    mRequest = HttpRequest();
    mChunkedReader.reset();
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

    mRequest.method = line.substr(0, methodEnd);
    mRequest.uri = line.substr(methodEnd + 1, uriEnd - methodEnd - 1);
    mRequest.version = line.substr(uriEnd + 1);

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

        fl::string name = trim(line.substr(0, colonPos));
        fl::string value = trim(line.substr(colonPos + 1));

        
        mRequest.headers[name] = value;
        
    }
}

void HttpRequestParser::parseBody() {
    if (mIsChunked) {
        // Feed buffer to chunked reader
        if (!mBuffer.empty()) {
            mChunkedReader.feed(mBuffer.data(), mBuffer.size());
            mBuffer.clear();
        }

        // Read all available chunks
        while (mChunkedReader.hasChunk()) {
            auto chunk = mChunkedReader.readChunk();
            if (chunk.has_value()) {
                mRequest.body.insert(mRequest.body.end(),
                                     chunk.value().begin(),
                                     chunk.value().end());
            }
        }

        // Check if final chunk received
        if (mChunkedReader.isFinal()) {
            mState = COMPLETE;
        }
    } else {
        // Read Content-Length bytes
        if (mBuffer.size() >= mContentLength) {
            mRequest.body.insert(mRequest.body.end(),
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

    for (const auto& pair : mRequest.headers) {
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
    , mContentLength(0)
    , mIsChunked(false)
{
}

void HttpResponseParser::feed(const u8* data, size_t len) {
    mBuffer.insert(mBuffer.end(), data, data + len);

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

fl::optional<HttpResponse> HttpResponseParser::getResponse() {
    if (mState != COMPLETE) {
        return fl::nullopt;
    }

    
    HttpResponse result = mResponse;
    
    reset();
    return result;
}

void HttpResponseParser::reset() {
    mState = READ_STATUS_LINE;
    mBuffer.clear();
    mResponse = HttpResponse();
    mChunkedReader.reset();
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

    mResponse.version = line.substr(0, versionEnd);
    fl::string statusStr = line.substr(versionEnd + 1, statusEnd - versionEnd - 1);

    if (!parseInt(statusStr, mResponse.statusCode)) {
        return false;  // Invalid status code
    }

    if (statusEnd < line.size()) {
        mResponse.reasonPhrase = line.substr(statusEnd + 1);
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

        fl::string name = trim(line.substr(0, colonPos));
        fl::string value = trim(line.substr(colonPos + 1));

        
        mResponse.headers[name] = value;
        
    }
}

void HttpResponseParser::parseBody() {
    if (mIsChunked) {
        // Feed buffer to chunked reader
        if (!mBuffer.empty()) {
            mChunkedReader.feed(mBuffer.data(), mBuffer.size());
            mBuffer.clear();
        }

        // Read all available chunks
        while (mChunkedReader.hasChunk()) {
            auto chunk = mChunkedReader.readChunk();
            if (chunk.has_value()) {
                mResponse.body.insert(mResponse.body.end(),
                                      chunk.value().begin(),
                                      chunk.value().end());
            }
        }

        // Check if final chunk received
        if (mChunkedReader.isFinal()) {
            mState = COMPLETE;
        }
    } else {
        // Read Content-Length bytes
        if (mBuffer.size() >= mContentLength) {
            mResponse.body.insert(mResponse.body.end(),
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

    for (const auto& pair : mResponse.headers) {
        if (toLower(pair.first) == lowerName) {
            return pair.second;
        }
    }

    return fl::nullopt;
}

} // namespace fl
