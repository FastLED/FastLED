#pragma once

#include "chunked_encoding.h"
#include "fl/stl/string.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/cstdlib.h"

namespace fl {

// ChunkedReader implementation

ChunkedReader::ChunkedReader()
    : mState(READ_SIZE), mChunkSize(0), mBytesRead(0) {
}

void ChunkedReader::feed(const u8* data, size_t len) {
    mBuffer.insert(mBuffer.end(), data, data + len);

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

fl::optional<fl::vector<u8>> ChunkedReader::readChunk() {
    if (mChunks.empty()) {
        return fl::nullopt;
    }
    fl::vector<u8> chunk = mChunks.front();
    mChunks.erase(mChunks.begin());
    return chunk;
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

fl::vector<u8> ChunkedWriter::writeChunk(const u8* data, size_t len) {
    return formatChunk(data, len);
}

fl::vector<u8> ChunkedWriter::writeFinal() {
    // Final chunk: 0\r\n\r\n
    const char* final = "0\r\n\r\n";
    return fl::vector<u8>(reinterpret_cast<const u8*>(final), // ok reinterpret cast
                                reinterpret_cast<const u8*>(final) + 5); // ok reinterpret cast
}

fl::vector<u8> ChunkedWriter::formatChunk(const u8* data, size_t len) {
    // Format: <size-hex>\r\n<data>\r\n
    char sizeHex[32];
    snprintf(sizeHex, sizeof(sizeHex), "%zx\r\n", len);

    fl::vector<u8> chunk;
    chunk.reserve(strlen(sizeHex) + len + 2);

    // Add size hex + CRLF
    chunk.insert(chunk.end(), reinterpret_cast<const u8*>(sizeHex), // ok reinterpret cast
                 reinterpret_cast<const u8*>(sizeHex) + strlen(sizeHex)); // ok reinterpret cast

    // Add data
    chunk.insert(chunk.end(), data, data + len);

    // Add trailing CRLF
    chunk.push_back('\r');
    chunk.push_back('\n');

    return chunk;
}

} // namespace fl
