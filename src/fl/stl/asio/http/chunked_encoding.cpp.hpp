#pragma once

#include "fl/stl/asio/http/chunked_encoding.h"
#include "fl/stl/string.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/cstdlib.h"

namespace fl {

// ChunkedReader implementation

ChunkedReader::ChunkedReader()
    : mState(READ_SIZE), mChunkSize(0), mBytesRead(0) {
}

void ChunkedReader::feed(fl::span<const u8> data) {
    mBuffer.insert(mBuffer.end(), data.begin(), data.end());

    // Process buffer based on current state
    while (true) {
        switch (mState) {
        case READ_SIZE: {
            // Parse chunk size (hex) until CRLF
            if (parseChunkSize(mChunkSize)) {
                if (mChunkSize == 0) {
                    // Final chunk (size 0)
                    mState = FINAL;
                    return;
                } else {
                    // Start reading chunk data
                    mState = READ_DATA;
                    mBytesRead = 0;
                }
            } else {
                // Not enough data to parse size
                return;
            }
            break;
        }
        case READ_DATA: {
            // Read chunk data (mChunkSize bytes)
            size_t remaining = mChunkSize;
            size_t available = mBuffer.size();
            if (available >= remaining) {
                // Complete chunk data available, save to mCurrentChunk
                mCurrentChunk.clear();
                mCurrentChunk.reserve(remaining);
                for (size_t i = 0; i < remaining; i++) {
                    mCurrentChunk.push_back(mBuffer[i]);
                }
                consume(remaining);
                mState = READ_TRAILER;
            } else {
                // Not enough data yet
                return;
            }
            break;
        }
        case READ_TRAILER: {
            // Read trailing CRLF
            if (hasCRLF()) {
                consume(2);  // Consume CRLF
                // Chunk is now complete, add to mChunks
                mChunks.push_back(mCurrentChunk);
                mCurrentChunk.clear();
                mState = READ_SIZE;
            } else {
                // Not enough data for CRLF
                return;
            }
            break;
        }
        case FINAL:
            // No more processing
            return;
        }
    }
}

bool ChunkedReader::hasChunk() const {
    return !mChunks.empty();
}

ChunkedReadResult ChunkedReader::readChunk(fl::span<u8> out) {
    using Status = ChunkedReadResult::Status;
    if (mChunks.empty()) {
        Status s = isFinal() ? Status::FINAL : Status::NO_DATA;
        return ChunkedReadResult(s, fl::span<const u8>());
    }
    const fl::vector<u8>& front = mChunks.front();
    if (out.size() < front.size()) {
        // Caller's buffer is too small
        return ChunkedReadResult(Status::NO_DATA, fl::span<const u8>());
    }
    memcpy(out.data(), front.data(), front.size());
    fl::span<const u8> written(out.data(), front.size());
    mChunks.erase(mChunks.begin());
    return ChunkedReadResult(Status::DATA, written);
}

size_t ChunkedReader::nextChunkSize() const {
    if (mChunks.empty()) {
        return 0;
    }
    return mChunks.front().size();
}

bool ChunkedReader::isFinal() const {
    return mState == FINAL;
}

void ChunkedReader::reset() {
    mState = READ_SIZE;
    mBuffer.clear();
    mChunkSize = 0;
    mBytesRead = 0;
    mChunks.clear();
    mCurrentChunk.clear();
}

bool ChunkedReader::parseChunkSize(size_t& outSize) {
    // Find CRLF in buffer
    for (size_t i = 0; i + 1 < mBuffer.size(); i++) {
        if (mBuffer[i] == '\r' && mBuffer[i + 1] == '\n') {
            // Found CRLF, parse hex size
            fl::string sizeStr(reinterpret_cast<const char*>(mBuffer.data()), i); // ok reinterpret cast

            // Parse hex (ignore chunk extensions after ';')
            size_t semicolon = sizeStr.find(';');
            if (semicolon != fl::string::npos) {
                sizeStr = sizeStr.substr(0, semicolon);
            }

            // Convert hex to size_t
            char* end = nullptr;
            outSize = static_cast<size_t>(strtoul(sizeStr.c_str(), &end, 16));
            if (end == sizeStr.c_str()) {
                // Invalid hex
                return false;
            }

            // Consume size line + CRLF
            consume(i + 2);
            return true;
        }
    }
    // No CRLF found
    return false;
}

bool ChunkedReader::hasCRLF() const {
    return mBuffer.size() >= 2 && mBuffer[0] == '\r' && mBuffer[1] == '\n';
}

void ChunkedReader::consume(size_t n) {
    if (n >= mBuffer.size()) {
        mBuffer.clear();
    } else {
        // fl::vector doesn't have erase(first, last), so create new vector without first n elements
        fl::vector<u8> newBuffer;
        newBuffer.reserve(mBuffer.size() - n);
        for (size_t i = n; i < mBuffer.size(); i++) {
            newBuffer.push_back(mBuffer[i]);
        }
        mBuffer = fl::move(newBuffer);
    }
}

// ChunkedWriter implementation

ChunkedWriter::ChunkedWriter() {
}

size_t ChunkedWriter::chunkOverhead(size_t dataLen) {
    // Overhead: hex-digits + "\r\n" + data + "\r\n"
    // Count hex digits needed
    char sizeHex[32];
    int hexLen = snprintf(sizeHex, sizeof(sizeHex), "%zx", dataLen);
    return static_cast<size_t>(hexLen) + 2 + dataLen + 2; // hex + \r\n + data + \r\n
}

size_t ChunkedWriter::writeChunk(fl::span<const u8> data, fl::span<u8> out) {
    // Format: <size-hex>\r\n<data>\r\n
    char sizeHex[32];
    int hexLen = snprintf(sizeHex, sizeof(sizeHex), "%zx\r\n", data.size());
    size_t totalNeeded = static_cast<size_t>(hexLen) + data.size() + 2;
    if (out.size() < totalNeeded) {
        return 0;
    }

    u8* dst = out.data();

    // Write size hex + CRLF
    memcpy(dst, sizeHex, static_cast<size_t>(hexLen));
    dst += hexLen;

    // Write data
    memcpy(dst, data.data(), data.size());
    dst += data.size();

    // Write trailing CRLF
    *dst++ = '\r';
    *dst++ = '\n';

    return totalNeeded;
}

size_t ChunkedWriter::writeFinal(fl::span<u8> out) {
    if (out.size() < FINAL_SIZE) {
        return 0;
    }
    // Final chunk: 0\r\n\r\n
    const char* finalStr = "0\r\n\r\n";
    memcpy(out.data(), finalStr, FINAL_SIZE);
    return FINAL_SIZE;
}

} // namespace fl
